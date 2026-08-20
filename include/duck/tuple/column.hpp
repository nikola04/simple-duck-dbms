#pragma once

#include <span>
#include <string_view>
#include <vector>
namespace duck {

enum class TypeId : std::uint16_t {
    INT32,
    FLOAT,
    BOOL,
    CHAR,
    VARCHAR,
};

// [name_length..2 bytes]
// [name...]
// [type..2 bytes] [length..2 bytes]
class Column {
public:
    Column(std::string name, TypeId type, std::uint16_t length = 0)
        : name_(std::move(name)), type_(type), length_(length) {
    }

    bool operator==(const Column& other) const {
        return type_ == other.type_ && length_ == other.length_ && name_ == other.name_;
    }

    std::string_view name() const {
        return name_;
    }
    TypeId type() const {
        return type_;
    }
    std::uint16_t length() const {
        return length_;
    }

    std::vector<std::byte> serialize() const;
    static std::pair<Column, std::uint16_t> deserialize(std::span<const std::byte> raw);

    bool is_fixed_size() const {
        return type_ == TypeId::INT32 || type_ == TypeId::FLOAT || type_ == TypeId::BOOL || type_ == TypeId::CHAR;
    }

private:
    std::string name_;
    TypeId type_;
    std::uint16_t length_; // fixed size for CHAR; ignored/max-hint for VARCHAR; 0 for INT32/FLOAT/BOOL
};

} // namespace duck