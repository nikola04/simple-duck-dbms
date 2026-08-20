#include "duck/tuple/schema.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace duck {

std::uint16_t Schema::fixed_size_of(size_t index) const {
    switch (const Column& column{this->column(index)}; column.type()) {
    case TypeId::BOOL:
        return 1;
    case TypeId::FLOAT:
        return 4;
    case TypeId::INT32:
        return 4;
    case TypeId::CHAR:
        return column.length();
    case TypeId::VARCHAR:
        return 0;
    }
    throw std::runtime_error("Schema::fixed_size_of: unknown typeId");
}

bool Schema::compatible_with(const Schema& other) const {
    if (this == &other)
        return true;

    return std::ranges::equal(columns_, other.columns_);
}

std::vector<std::byte> Schema::serialize() const {
    std::uint16_t n{static_cast<std::uint16_t>(column_count())};
    std::vector<std::byte> out;
    out.reserve(n * 6 + 2); // at least 6 bytes per column and 2 for size

    auto size_bytes{std::bit_cast<std::array<std::byte, 2>>(n)};
    out.insert(out.end(), size_bytes.begin(), size_bytes.end());

    for (std::uint16_t i{0}; i < n; ++i) {
        auto bytes{columns_[i].serialize()};
        out.insert(out.end(), bytes.begin(), bytes.end());
    };

    return out;
}

std::vector<Column> Schema::deserialize(std::span<const std::byte> raw) {
    if (raw.size() < 2) {
        throw std::runtime_error("Schema::deserialize: raw data too small for column count");
    }

    std::array<std::byte, 2> length_arr;
    memcpy(length_arr.data(), raw.data(), 2);
    std::uint16_t column_count{std::bit_cast<std::uint16_t>(length_arr)};

    std::vector<Column> columns;
    columns.reserve(column_count);

    std::uint16_t offset{2};

    for (std::uint16_t i{0}; i < column_count; ++i) {
        if (raw.size() < offset) {
            throw std::runtime_error(
                std::format("Schema::deserialize: raw data too small for specified column count ({})", column_count));
        }
        auto deserialized{Column::deserialize(raw.subspan(offset))};
        columns.push_back(deserialized.first);
        offset += deserialized.second;
    }

    return columns;
}

std::string Schema::to_string() const {
    std::string s;
    size_t n{columns_.size()};
    s.reserve(n * 4);

    for (size_t i{0}; i < n; ++i) {
        switch (columns_[i].type()) {
        case TypeId::INT32:
            s += "INT32";
            break;
        case TypeId::FLOAT:
            s += "FLOAT";
            break;
        case TypeId::BOOL:
            s += "BOOL";
            break;
        case TypeId::VARCHAR:
            s += "VARCHAR";
            break;
        case TypeId::CHAR:
            s += "CHAR(" + std::to_string(columns_[i].length()) + ")";
            break;
        }
        if (i != n - 1)
            s += " | ";
    }

    return s;
}

} // namespace duck