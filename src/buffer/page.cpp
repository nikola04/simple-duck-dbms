#include "duck/buffer/page.hpp"
#include "duck/config/sizes.hpp"
#include "duck/storage/disk_manager.hpp"
#include <cstring>

namespace duck {

void Page::reset_memory() {
    memset(data_, 0, kPAGE_SIZE);

    page_id_ = INVALID_PAGE_ID;
    is_dirty_ = false;
    pin_count_ = 0;
}

} // namespace duck