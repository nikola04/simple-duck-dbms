#pragma once

#include "duck/buffer/page.hpp"
#include "duck/storage/disk_manager.hpp"
#include <list>
#include <unordered_map>

namespace duck {

class BufferReplacer {
public:
    explicit BufferReplacer(const Page* frames, const std::unordered_map<PageID, FrameID>& page_table);

    void record(PageID page_id);
    void remove(PageID page_id);
    PageID find_lru();

private:
    const Page* frames_;
    const std::unordered_map<PageID, FrameID>& page_table_{};

    std::list<PageID> mru_pages_{};
    std::unordered_map<PageID, std::list<PageID>::const_iterator> positions_{};
};

} // namespace duck