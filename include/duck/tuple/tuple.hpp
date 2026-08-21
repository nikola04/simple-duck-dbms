#pragma once

#include "duck/tuple/schema.hpp"
#include "duck/tuple/value.hpp"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace duck {

// [NULL bitmap: ceil(num_cols/8) bytes]
// [fixed-size columns, in schema order]
// [varlen columns: 2-byte length prefix + bytes, in schema order]
class Tuple {
public:
    // Constructs a Tuple by deserializing the supplied raw tuple data.
    // The input data is not retained after construction.
    Tuple(std::span<const std::byte> raw, const Schema& schema);
    Tuple(std::vector<Value> values, const Schema& schema);

    const Schema& schema() const {
        return schema_;
    }
    Value get(size_t column_index) const {
        return values_.at(column_index);
    }
    Value get(std::string_view name) const {
        auto idx = schema_.column_index(name);
        if (!idx.has_value())
            throw std::runtime_error("Tuple::get: no such column: " + std::string(name));

        return values_.at(idx.value());
    }

    std::vector<std::byte> serialize() const;
    std::string to_string() const; // debug helper

private:
    const Schema& schema_;
    std::vector<Value> values_;

    std::vector<Value> deserialize(std::span<const std::byte> raw);
    Value deserialize_value(std::span<const std::byte> raw, const std::vector<std::uint16_t>& offsets,
                            size_t column_index);
    std::vector<std::uint16_t> compute_offsets(std::span<const std::byte> raw) const;
    std::uint16_t read_length(std::span<const std::byte> raw, std::uint16_t offset) const;
};

class NullBitmap {
public:
    static std::size_t size(std::size_t column_count) {
        return (column_count + 7) / 8;
    }
    static std::bitset<8> get_bitset(std::span<const std::byte> bitmap, std::size_t column);

    static bool is_null(std::span<const std::byte> bitmap, std::size_t column);
    static void set_null(std::span<std::byte> bitmap, std::size_t column);
};

} // namespace duck