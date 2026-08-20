#include "duck/tuple/column.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace duck {

std::vector<std::byte> Column::serialize() const {
    std::vector<std::byte> out;
    out.reserve(name_.length() + 6);

    auto name_length_bytes{std::bit_cast<std::array<std::byte, 2>>(static_cast<uint16_t>(name_.length()))};
    out.insert(out.end(), name_length_bytes.begin(), name_length_bytes.end());

    auto name_bytes{std::as_bytes(std::span(name_))};
    out.insert(out.end(), name_bytes.begin(), name_bytes.end());

    auto type_bytes{std::bit_cast<std::array<std::byte, 2>>(type_)};
    out.insert(out.end(), type_bytes.begin(), type_bytes.end());

    auto length_bytes{std::bit_cast<std::array<std::byte, 2>>(length_)};
    out.insert(out.end(), length_bytes.begin(), length_bytes.end());

    return out;
}

std::pair<Column, std::uint16_t> Column::deserialize(std::span<const std::byte> raw) {
    if (raw.size() < 2) {
        throw std::runtime_error("Column::deserialize: raw data too small for name length");
    }

    std::array<std::byte, 2> name_length_arr;
    memcpy(name_length_arr.data(), raw.data(), 2);
    std::uint16_t name_length{std::bit_cast<std::uint16_t>(name_length_arr)};

    size_t offset{2};

    if (raw.size() < offset + name_length + 4) {
        throw std::runtime_error("Column::deserialize: raw data too small for column body");
    }

    auto name_span{raw.subspan(offset, name_length)};
    std::string name{reinterpret_cast<const char*>(name_span.data()), name_span.size()};

    offset += name_length;

    std::array<std::byte, 2> type_arr;
    memcpy(type_arr.data(), raw.subspan(offset, 2).data(), 2);
    TypeId type{std::bit_cast<TypeId>(type_arr)};

    offset += 2;

    std::array<std::byte, 2> length_arr;
    memcpy(length_arr.data(), raw.subspan(offset, 2).data(), 2);
    std::uint16_t length{std::bit_cast<std::uint16_t>(length_arr)};

    return std::pair{Column{std::move(name), type, length}, offset + 2};
}

} // namespace duck