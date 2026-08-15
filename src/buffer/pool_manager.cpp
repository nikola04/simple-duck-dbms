#include "duck/buffer/pool_manager.hpp"
#include "duck/buffer/replacer.hpp"
#include "duck/storage/disk_manager.hpp"
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace duck {

BufferPoolManager::BufferPoolManager(DiskManager& disk_manager, size_t pool_capacity)
    : disk_manager_(disk_manager), pool_capacity_(pool_capacity), frames_(std::make_unique<Page[]>(pool_capacity_)),
      replacer_(frames_.get(), page_table_) {
}

BufferPoolManager::~BufferPoolManager() {
    flush_all();
}

Page* BufferPoolManager::new_page() {
    PageID page_id = disk_manager_.allocate_page();

    return swap_page(page_id, false);
}

Page* BufferPoolManager::fetch_page(const PageID page_id) {
    if (Page* page = find_cached_page(page_id); page != nullptr)
        return page;

    return swap_page(page_id);
}

bool BufferPoolManager::delete_page(const PageID page_id) {
    std::unique_lock<std::shared_mutex> lock{latch_};

    if (auto it = page_table_.find(page_id); it != page_table_.end()) {
        FrameID frame_id = it->second;
        Page& page{frames_[frame_id]};

        std::unique_lock<std::shared_mutex> page_lock{page.latch()};

        if (page.pin_count() > 0)
            return false;

        page_table_.erase(it);
        replacer_.remove(page_id);
        page.reset_memory();

        std::unique_lock<std::mutex> lock{free_frames_list_latch_};
        free_frames_list_.push_back(frame_id);
    }

    disk_manager_.deallocate_page(page_id);
    return true;
}

Page* BufferPoolManager::find_cached_page(PageID page_id) {
    std::unique_lock<std::shared_mutex> lock{latch_}; // for now will be unique because of replacer.record

    if (auto it = page_table_.find(page_id); it != page_table_.end()) {
        FrameID frame_id = it->second;
        Page& page{frames_[frame_id]};

        std::unique_lock<std::shared_mutex> page_lock{page.latch()};

        page.inc_pin_count();
        replacer_.record(page_id);
        return &page;
    }

    return nullptr;
}

void BufferPoolManager::flush_page(Page& page) {
    disk_manager_.write_page(page.page_id(), page.data());
}

Page* BufferPoolManager::swap_page(PageID page_id, bool read_from_disk) {
    std::unique_lock<std::shared_mutex> lock{latch_};

    FrameID frame_id = find_next_frame();
    if (frame_id == INVALID_FRAME_ID)
        return nullptr;

    Page& page{frames_[frame_id]};
    std::unique_lock<std::shared_mutex> page_lock{page.latch()};

    if (page.page_id() != INVALID_PAGE_ID) {
        page_table_.erase(page.page_id());
        replacer_.remove(page.page_id());
    }

    if (page.is_dirty()) {
        flush_page(page);
    }

    page_table_[page_id] = frame_id;
    page.reset_memory(page_id);
    page.inc_pin_count();
    replacer_.record(page_id);

    lock.unlock();
    if (read_from_disk)
        disk_manager_.read_page(page_id, page.data());

    return &page;
}

void BufferPoolManager::unpin_page(const PageID page_id, bool dirty) {
    std::shared_lock<std::shared_mutex> lock{latch_};

    std::cout << "unpinning: " << page_id << " " << dirty << "\n";

    if (const auto it = page_table_.find(page_id); it != page_table_.end()) {
        FrameID frame_id = it->second;
        Page& page{frames_[frame_id]};

        std::unique_lock<std::shared_mutex> page_lock{page.latch()};

        if (dirty)
            page.set_dirty(true);

        page.dec_pin_count();
    }
}

FrameID BufferPoolManager::find_next_frame() {
    if (std::unique_lock<std::mutex> lock{free_frames_list_latch_}; !free_frames_list_.empty()) {
        FrameID frame_id{free_frames_list_.back()};
        free_frames_list_.pop_back();

        return frame_id;
    }

    if (pool_size_ < pool_capacity_)
        return pool_size_++;

    // find frame to replace
    if (PageID page_id = replacer_.find_lru(); page_id != INVALID_PAGE_ID) {
        return page_table_[page_id];
    }

    return INVALID_FRAME_ID;
}

void BufferPoolManager::flush_all() {
    std::unique_lock<std::shared_mutex> lock{latch_};

    for (FrameID id{0}; id < pool_size_; ++id) {
        if (Page& page{frames_[id]}; page.is_dirty())
            flush_page(page);
    }
}

} // namespace duck