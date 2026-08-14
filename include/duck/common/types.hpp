#include <cstdint>
#include <limits>

namespace duck {

using PageID = std::uint32_t;
inline constexpr PageID INVALID_PAGE_ID = std::numeric_limits<PageID>::max();

using FrameID = std::uint32_t;
inline constexpr FrameID INVALID_FRAME_ID = std::numeric_limits<FrameID>::max();

} // namespace duck