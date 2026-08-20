#include "duck/buffer/page.hpp"
#include "duck/config/sizes.hpp"
#include <cstring>

namespace duck {

void Page::reset_memory(PageID page_id) {
    memset(data_.data(), 0, kPAGE_SIZE);

    page_id_ = page_id;
    is_dirty_ = false;
    pin_count_ = 0;
}

} // namespace duck