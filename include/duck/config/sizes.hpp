#pragma once

#include <cstddef>
#include <cstdint>

namespace duck {

inline constexpr std::size_t kPAGE_SIZE = 8192;
inline constexpr std::size_t kMAX_PAGES = 1000;

constexpr std::uint16_t DEFAULT_VARCHAR_LENGTH = 256;

} // namespace duck