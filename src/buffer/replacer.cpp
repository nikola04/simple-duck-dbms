#include "duck/buffer/replacer.hpp"

namespace duck {

BufferReplacer::BufferReplacer(const Page* frames, const std::unordered_map<PageID, FrameID>& page_table)
    : frames_(frames), page_table_(page_table) {
}

void BufferReplacer::record(PageID page_id) {
    remove(page_id);

    auto it = mru_pages_.insert(mru_pages_.cend(), page_id);
    positions_[page_id] = it;
}

void BufferReplacer::remove(PageID page_id) {
    if (auto it = positions_.find(page_id); it != positions_.end()) {
        mru_pages_.erase(it->second);
        positions_.erase(it);
    }
}

PageID BufferReplacer::find_lru() {
    for (auto it = mru_pages_.cbegin(); it != mru_pages_.cend(); it++) {
        const PageID page_id = *it;
        if (auto mit = page_table_.find(page_id); mit != page_table_.end()) {
            const FrameID frame_id = mit->second;
            if (frames_[frame_id].pin_count() == 0)
                return page_id;
        }
    }

    return INVALID_PAGE_ID;
}

} // namespace duck