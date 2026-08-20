#include "duck/tuple/tuple.hpp"
#include "duck/tuple/schema.hpp"
#include "duck/tuple/value.hpp"
#include <array>
#include <bit>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace duck {

Tuple::Tuple(std::span<const std::byte> raw, const Schema& schema) : schema_(schema), values_(deserialize(raw)) {
}

Tuple::Tuple(std::vector<Value> values, const Schema& schema) : schema_(schema), values_(std::move(values)) {
    if (values_.size() != schema.column_count())
        throw std::runtime_error("Tuple::Tuple: value vector size doesn't match schema column count");
    for (size_t i{0}; i < values_.size(); ++i) {
        const Value& v{values_[i]};
        const Column& c{schema_.column(i)};
        if (v.type() != type_id_to_value_type(c.type))
            throw std::runtime_error(std::format("Tuple::Tuple: value type {} is not supported by column type {}",
                                                 static_cast<int>(v.type()), static_cast<int>(c.type)));
    }
}

std::vector<Value> Tuple::deserialize(std::span<const std::byte> raw) {
    auto offsets{compute_offsets(raw)};

    std::vector<Value> values;
    values.reserve(schema_.column_count());
    for (size_t column{0}; column < schema_.column_count(); column++) {
        values.push_back(deserialize_value(raw, offsets, column));
    }

    return values;
}

Value Tuple::deserialize_value(std::span<const std::byte> raw, const std::vector<std::uint16_t>& offsets,
                               size_t column_index) {
    if (bool is_null{NullBitmap::is_null(raw, column_index)}; is_null) {
        return Value::null(type_id_to_value_type(schema_.column(column_index).type));
    }

    std::uint16_t offset = offsets[column_index];
    const Column& column{schema_.column(column_index)};

    std::size_t size{schema_.fixed_size_of(column_index)};
    if (!column.is_fixed_size()) {
        size = read_length(raw, offset);
        offset += 2;
    }

    std::span<const std::byte> value_bytes{raw.subspan(offset, size)};

    switch (column.type) {
    case TypeId::INT32: {
        std::array<std::byte, 4> bytes;
        std::memcpy(bytes.data(), value_bytes.data(), 4);
        return Value::of(std::bit_cast<std::int32_t>(bytes));
    }
    case TypeId::FLOAT: {
        std::array<std::byte, 4> bytes;
        std::memcpy(bytes.data(), value_bytes.data(), 4);
        return Value::of(std::bit_cast<float>(bytes));
    }
    case TypeId::BOOL: {
        return Value::of(value_bytes[0] != std::byte{0});
    }
    case TypeId::CHAR: {
        std::string s{std::string(reinterpret_cast<const char*>(value_bytes.data()), value_bytes.size())};

        if (std::size_t last_c{s.find_last_not_of('\0')}; last_c != std::string::npos) {
            s.resize(last_c + 1);
        } else {
            s.clear();
        }
        return Value::of(std::move(s));
    }
    case TypeId::VARCHAR: {
        return Value::of(std::string(reinterpret_cast<const char*>(value_bytes.data()), value_bytes.size()));
    }
    default:
        throw std::runtime_error("Tuple::get: unknown TypeId");
    }
}

std::vector<std::uint16_t> Tuple::compute_offsets(std::span<const std::byte> raw) const {
    std::vector<std::uint16_t> offsets{};
    offsets.reserve(schema_.column_count());

    size_t offset{NullBitmap::size(schema_.column_count())};

    for (size_t i{0}; i < schema_.column_count(); i++) {
        offsets.push_back(offset);

        std::uint16_t fixed{schema_.fixed_size_of(i)};
        if (fixed > 0)
            offset += fixed;
        else
            offset += read_length(raw, offset) + 2; // +2 bytes for size
    }

    return offsets;
}

std::uint16_t Tuple::read_length(std::span<const std::byte> raw, uint16_t offset) const {
    std::array<std::byte, 2> bytes{};
    std::memcpy(bytes.data(), raw.data() + offset, 2);
    return std::bit_cast<uint16_t>(bytes);
}

std::vector<std::byte> Tuple::serialize() const {
    std::vector<std::byte> out{};
    std::size_t bitmap_size{NullBitmap::size(schema_.column_count())};
    out.resize(bitmap_size, std::byte{0});

    for (size_t i{0}; i < schema_.column_count(); ++i) {
        if (values_[i].is_null())
            NullBitmap::set_null(out, i);
    }

    for (size_t i{0}; i < schema_.column_count(); ++i) {
        const Column& column{schema_.column(i)};

        if (values_[i].is_null()) {
            // space needs to be reserved anyway
        } else if (column.type == TypeId::CHAR || column.type == TypeId::VARCHAR) {
            size_t value_size = values_[i].serialized_size();
            // column.length -> CHAR=SIZE, VARCHAR=MAX_SIZE
            if (value_size > column.length)
                throw std::runtime_error(
                    std::format("Tuple::serialize: value size exceeds column's size (value: {}, column: {}, type: {})",
                                value_size, column.length, static_cast<uint16_t>(column.type)));
        }

        if (column.is_fixed_size()) {
            std::uint16_t fixed{schema_.fixed_size_of(i)};

            std::size_t before = out.size();
            values_[i].serialize_to(out);
            std::size_t written = out.size() - before;

            // add padding if needed
            if (written < fixed) {
                out.resize(out.size() + (fixed - written), std::byte{0});
            }
        } else {
            auto size_bytes{
                std::bit_cast<std::array<std::byte, 2>>(static_cast<uint16_t>(values_[i].serialized_size()))};
            out.insert(out.end(), size_bytes.begin(), size_bytes.end());
            values_[i].serialize_to(out);
        }
    }

    return out;
}

std::bitset<8> NullBitmap::get_bitset(std::span<const std::byte> bitmap, std::size_t column) {
    std::cout << bitmap.size() << "\n";
    std::byte byte = bitmap[column / 8];

    std::bitset<8> bits{std::to_integer<unsigned char>(byte)};
    return bits;
}

bool NullBitmap::is_null(std::span<const std::byte> bitmap, std::size_t column) {
    std::bitset<8> bits{get_bitset(bitmap, column)};

    uint16_t bit_idx = column % 8;

    return bits[bit_idx];
}

void NullBitmap::set_null(std::span<std::byte> bitmap, std::size_t column) {
    const std::size_t byte_index = column / 8;
    const std::size_t bit_index = column % 8;

    bitmap[byte_index] |= std::byte{1} << bit_index;
}

} // namespace duck