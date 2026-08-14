#pragma once

#include "duck/buffer/page.hpp"
#include "duck/buffer/replacer.hpp"
#include "duck/storage/disk_manager.hpp"
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

namespace duck {

class BufferPoolManager {
public:
    BufferPoolManager(DiskManager& disk_manager, size_t pool_capacity);
    ~BufferPoolManager();

    Page* new_page();
    Page* fetch_page(const PageID page_id);
    bool delete_page(const PageID page_id);

    void unpin_page(const PageID page_id, bool dirty = false);

    void flush_all();

private:
    DiskManager& disk_manager_;

    size_t pool_capacity_;
    size_t pool_size_{0};
    std::unique_ptr<Page[]> frames_;
    std::unordered_map<PageID, FrameID> page_table_{};
    std::shared_mutex latch_{};
    std::vector<FrameID> free_frames_list_{};
    std::mutex free_frames_list_latch_{};

    FrameID find_next_frame();
    Page* find_cached_page(PageID page_id);
    void flush_page(Page& page);
    Page* swap_page(PageID page_id, bool read_from_disk = true);

    BufferReplacer replacer_;
};

} // namespace duck