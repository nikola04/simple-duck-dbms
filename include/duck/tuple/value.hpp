#pragma once

#include "duck/tuple/column.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace duck {

enum class ValueType { INT64, UINT64, INT32, UINT32, DOUBLE, FLOAT, BOOL, STRING, BYTES };

constexpr ValueType type_id_to_value_type(TypeId typeId) {
    switch (typeId) {
    case TypeId::BOOL:
        return ValueType::BOOL;
    case TypeId::DOUBLE:
        return ValueType::DOUBLE;
    case TypeId::FLOAT:
        return ValueType::FLOAT;
    case TypeId::INT64:
        return ValueType::INT64;
    case TypeId::UINT64:
        return ValueType::UINT64;
    case TypeId::INT32:
        return ValueType::INT32;
    case TypeId::UINT32:
        return ValueType::UINT32;
    case TypeId::CHAR:
    case TypeId::VARCHAR:
        return ValueType::STRING;
    case TypeId::VARBINARY:
        return ValueType::BYTES;
    }
    throw std::runtime_error("type_id_to_value_type: invalid type id");
}

using value_variant = std::variant<std::uint64_t, std::int64_t, std::uint32_t, std::int32_t, double, float, bool,
                                   std::string, std::vector<std::byte>>;

class Value {
public:
    Value(ValueType type, bool is_null, value_variant data);

    static Value null(ValueType type);
    static Value of(std::int64_t v);
    static Value of(std::uint64_t v);
    static Value of(std::int32_t v);
    static Value of(std::uint32_t v);
    static Value of(double v);
    static Value of(float v);
    static Value of(bool v);
    static Value of(std::string v); // should be used for both CHAR and VARCHAR
    static Value of(std::vector<std::byte> v);

    bool is_null() const;
    ValueType type() const;

    std::int64_t as_int64() const;
    std::uint64_t as_uint64() const;
    std::int32_t as_int32() const;
    std::uint32_t as_uint32() const;
    double as_double() const;
    float as_float() const;
    bool as_bool() const;
    const std::string& as_string() const;
    const std::vector<std::byte>& as_bytes() const;

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

    value_variant data_;
};

} // namespace duck