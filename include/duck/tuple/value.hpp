#pragma once

#include "duck/tuple/column.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace duck {

enum class ValueType { INT32, FLOAT, BOOL, STRING };

constexpr ValueType type_id_to_value_type(TypeId typeId) {
    switch (typeId) {
    case TypeId::BOOL:
        return ValueType::BOOL;
    case TypeId::FLOAT:
        return ValueType::FLOAT;
    case TypeId::INT32:
        return ValueType::INT32;
    case TypeId::CHAR:
    case TypeId::VARCHAR:
        return ValueType::STRING;
    default:
        throw std::runtime_error("type_id_to_value_type: invalid type id");
    }
}

class Value {
public:
    Value(ValueType type, bool is_null, std::variant<std::int32_t, float, bool, std::string> data);

    static Value null(ValueType type);
    static Value of(std::int32_t v);
    static Value of(float v);
    static Value of(bool v);
    static Value of(std::string v); // should be used for both CHAR and VARCHAR

    bool is_null() const;
    ValueType type() const;

    std::int32_t as_int32() const;
    float as_float() const;
    bool as_bool() const;
    const std::string& as_string() const;

    std::string to_string() const;

    std::size_t serialized_size() const;
    void serialize_to(std::vector<std::byte>& out) const;

    bool operator==(const Value& other) {
        if (is_null_ != other.is_null_)
            return false;
        if (is_null_)
            return type_ == other.type_;
        return type_ == other.type_ && data_ == other.data_;
    }

private:
    ValueType type_;
    bool is_null_{false};

    std::variant<std::int32_t, float, bool, std::string> data_;
};

} // namespace duck