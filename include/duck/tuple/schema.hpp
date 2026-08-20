#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace duck {

enum class TypeId : std::uint16_t {
    INT32,
    FLOAT,
    BOOL,
    CHAR,
    VARCHAR,
};

struct Column {
    std::string name;
    TypeId type;
    std::uint16_t length{0}; // fixed size for CHAR; ignored/max-hint for VARCHAR; 0 for INT32/FLOAT/BOOL

    bool is_fixed_size() const {
        return type == TypeId::INT32 || type == TypeId::FLOAT || type == TypeId::BOOL || type == TypeId::CHAR;
    }
};

class Schema {
public:
    explicit Schema(std::vector<Column> columns) : columns_(std::move(columns)) {
    }

    const Column& column(size_t index) const {
        return columns_[index];
    }

    size_t column_count() const {
        return columns_.size();
    }

    std::uint16_t fixed_size_of(size_t index) const;

private:
    std::vector<Column> columns_;
};

} // namespace duck