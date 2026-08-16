#pragma once

#include "types.hpp"
#include <cstdint>

namespace duck {

struct RID {
    PageID page_id;
    std::uint16_t slot_num;

    bool operator==(const RID&) const = default;
};

} // namespace duck