#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace duck {

BufferPoolManager::BufferPoolManager(DiskManager& disk_manager, size_t pool_size)
    : disk_manager_(disk_manager), pool_size_(pool_size), frames_(std::make_unique<Page[]>(pool_size_)) {
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

Page* BufferPoolManager::find_cached_page(PageID page_id) {
    std::shared_lock<std::shared_mutex> lock{latch_};

    if (auto it = page_table_.find(page_id); it != page_table_.end()) {
        FrameID frame_id = it->second;
        Page& page{frames_[frame_id]};

        std::unique_lock<std::shared_mutex> page_lock{page.latch()};

        page.inc_pin_count();
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

    if (page.is_dirty()) {
        flush_page(page);
    }

    page_table_[page_id] = frame_id;
    page.reset_memory(page_id);
    page.inc_pin_count();

    lock.unlock();
    if (read_from_disk)
        disk_manager_.read_page(page_id, page.data());

    return &page;
}

void BufferPoolManager::unpin_page(const PageID page_id, bool dirty) {
    std::shared_lock<std::shared_mutex> lock{latch_};

    FrameID frame_id = page_table_[page_id];
    Page& page{frames_[frame_id]};

    std::unique_lock<std::shared_mutex> page_lock{page.latch()};

    if (dirty)
        page.set_dirty();

    page.dec_pin_count();
}

FrameID BufferPoolManager::find_next_frame() {
    return frames_capacity_++;
    // return INVALID_FRAME_ID;
}

void BufferPoolManager::flush_all() {
    std::unique_lock<std::shared_mutex> lock{latch_};

    for (FrameID id{0}; id < frames_capacity_; ++id) {
        Page& page{frames_[id]};
        if (!page.is_dirty())
            continue;

        flush_page(page);
    }
}

} // namespace duck