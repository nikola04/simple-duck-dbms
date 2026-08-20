#include "duck/tuple/value.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace duck {

Value::Value(ValueType type, bool is_null, std::variant<std::int32_t, float, bool, std::string> data)
    : type_(type), is_null_(is_null), data_(std::move(data)) {
}

Value Value::null(ValueType type) {
    return Value{type, true, {}};
}
Value Value::of(std::int32_t v) {
    return Value{ValueType::INT32, false, v};
}
Value Value::of(float v) {
    return Value{ValueType::FLOAT, false, v};
}
Value Value::of(bool v) {
    return Value{ValueType::BOOL, false, v};
}
Value Value::of(std::string v) {
    return Value{ValueType::STRING, false, std::move(v)};
}

bool Value::is_null() const {
    return is_null_;
}
ValueType Value::type() const {
    return type_;
}

std::int32_t Value::as_int32() const {
    if (is_null_)
        throw std::runtime_error("Value::as_int32: tried to read null value");
    return std::get<std::int32_t>(data_);
}
float Value::as_float() const {
    if (is_null_)
        throw std::runtime_error("Value::as_float: tried to read null value");
    return std::get<float>(data_);
}
bool Value::as_bool() const {
    if (is_null_)
        throw std::runtime_error("Value::as_bool: tried to read null value");
    return std::get<bool>(data_);
}
const std::string& Value::as_string() const {
    if (is_null_)
        throw std::runtime_error("Value::as_string: tried to read null value");
    return std::get<std::string>(data_);
}

std::size_t Value::serialized_size() const {
    switch (type_) {
    case ValueType::INT32:
    case ValueType::FLOAT:
        return 4;
    case ValueType::BOOL:
        return 1;
    case ValueType::STRING:
        return std::get<std::string>(data_).size();
    default:
        throw std::runtime_error("Value::serialized_size: type not found");
    }
}
void Value::serialize_to(std::vector<std::byte>& out) const {
    switch (type_) {
    case ValueType::INT32: {
        auto bytes{std::bit_cast<std::array<std::byte, 4>>(std::get<std::int32_t>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        break;
    }
    case ValueType::FLOAT: {
        auto bytes{std::bit_cast<std::array<std::byte, 4>>(std::get<float>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        break;
    }
    case ValueType::BOOL: {
        out.push_back(std::get<bool>(data_) ? std::byte{1} : std::byte{0});
        break;
    }
    case ValueType::STRING: {
        const std::string& s{std::get<std::string>(data_)};
        auto bytes{std::as_bytes(std::span(s))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        break;
    }
    default:
        throw std::runtime_error("Value::serialize_to: type not found");
    }
}

} // namespace duck