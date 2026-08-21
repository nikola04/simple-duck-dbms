#pragma once

#include "duck/tuple/column.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace duck {

// [column_count..2 bytes] [COLUMN...]
class Schema {
public:
    explicit Schema(std::vector<Column> columns) : columns_(std::move(columns)) {
        if (columns_.size() == 0)
            throw std::runtime_error("Schema::Schema: tried to create schema with 0 columns");
    }
    Schema(std::span<const std::byte> raw) : columns_(deserialize(raw)) {
    }

    const Column& column(size_t index) const {
        return columns_.at(index);
    }
    std::optional<size_t> column_index(std::string_view name) const {
        // TODO: consider caching column_index as unordered_map if profiling shows this as a bottleneck
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].name() == name) {
                return i;
            }
        }
        return std::nullopt;
    }

    size_t column_count() const {
        return columns_.size();
    }

    std::uint16_t fixed_size_of(size_t index) const;
    bool compatible_with(const Schema& other) const;

    std::vector<std::byte> serialize() const;

    std::string to_string() const; // debug method

private:
    std::vector<Column> columns_;

    static std::vector<Column> deserialize(std::span<const std::byte> raw);
};

} // namespace duck