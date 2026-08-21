#include "duck/tuple/value.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace duck {

Value::Value(ValueType type, bool is_null, value_variant data)
    : type_(type), is_null_(is_null), data_(std::move(data)) {
}

Value Value::null(ValueType type) {
    return Value{type, true, {}};
}
Value Value::of(std::int64_t v) {
    return Value{ValueType::INT64, false, v};
}
Value Value::of(std::uint64_t v) {
    return Value{ValueType::UINT64, false, v};
}
Value Value::of(std::int32_t v) {
    return Value{ValueType::INT32, false, v};
}
Value Value::of(std::uint32_t v) {
    return Value{ValueType::UINT32, false, v};
}
Value Value::of(double v) {
    return Value{ValueType::DOUBLE, false, v};
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
Value Value::of(std::vector<std::byte> v) {
    return Value{ValueType::BYTES, false, std::move(v)};
}

bool Value::is_null() const {
    return is_null_;
}
ValueType Value::type() const {
    return type_;
}

std::int64_t Value::as_int64() const {
    if (is_null_)
        throw std::runtime_error("Value::as_int64: tried to read null value");
    return std::get<std::int64_t>(data_);
}
std::uint64_t Value::as_uint64() const {
    if (is_null_)
        throw std::runtime_error("Value::as_uint64: tried to read null value");
    return std::get<std::uint64_t>(data_);
}
std::int32_t Value::as_int32() const {
    if (is_null_)
        throw std::runtime_error("Value::as_int32: tried to read null value");
    return std::get<std::int32_t>(data_);
}
std::uint32_t Value::as_uint32() const {
    if (is_null_)
        throw std::runtime_error("Value::as_uint32: tried to read null value");
    return std::get<std::uint32_t>(data_);
}
double Value::as_double() const {
    if (is_null_)
        throw std::runtime_error("Value::as_double: tried to read null value");
    return std::get<double>(data_);
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
const std::vector<std::byte>& Value::as_bytes() const {
    if (is_null_)
        throw std::runtime_error("Value::as_bytes: tried to read null value");
    return std::get<std::vector<std::byte>>(data_);
}

std::string Value::to_string() const {
    if (is_null_)
        return "null";

    switch (type_) {
    case ValueType::INT64:
        return std::to_string(as_int64());
    case ValueType::UINT64:
        return std::to_string(as_uint64());
    case ValueType::INT32:
        return std::to_string(as_int32());
    case ValueType::UINT32:
        return std::to_string(as_uint32());
    case ValueType::DOUBLE:
        return std::to_string(as_double());
    case ValueType::FLOAT:
        return std::to_string(as_float());
    case ValueType::BOOL:
        return as_bool() ? "true" : "false";
    case ValueType::STRING:
        return as_string();
    case ValueType::BYTES:
        return std::format("BYTES({})", as_bytes().size());
    }
    throw std::runtime_error("Value::to_string: type not found");
}

std::size_t Value::serialized_size() const {
    switch (type_) {
    case ValueType::INT64:
    case ValueType::UINT64:
    case ValueType::DOUBLE:
        return 8;
    case ValueType::INT32:
    case ValueType::UINT32:
    case ValueType::FLOAT:
        return 4;
    case ValueType::BOOL:
        return 1;
    case ValueType::STRING:
        return std::get<std::string>(data_).size();
    case ValueType::BYTES:
        return std::get<std::vector<std::byte>>(data_).size();
    }
    throw std::runtime_error("Value::serialized_size: type not found");
}
void Value::serialize_to(std::vector<std::byte>& out) const {
    switch (type_) {
    case ValueType::INT64: {
        auto bytes{std::bit_cast<std::array<std::byte, 8>>(std::get<std::int64_t>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case ValueType::UINT64: {
        auto bytes{std::bit_cast<std::array<std::byte, 8>>(std::get<std::uint64_t>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case ValueType::DOUBLE: {
        auto bytes{std::bit_cast<std::array<std::byte, 8>>(std::get<double>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case ValueType::INT32: {
        auto bytes{std::bit_cast<std::array<std::byte, 4>>(std::get<std::int32_t>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case ValueType::UINT32: {
        auto bytes{std::bit_cast<std::array<std::byte, 4>>(std::get<std::uint32_t>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case ValueType::FLOAT: {
        auto bytes{std::bit_cast<std::array<std::byte, 4>>(std::get<float>(data_))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case ValueType::BOOL: {
        out.push_back(std::get<bool>(data_) ? std::byte{1} : std::byte{0});
        return;
    }
    case ValueType::STRING: {
        const std::string& s{std::get<std::string>(data_)};
        auto bytes{std::as_bytes(std::span(s))};
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case ValueType::BYTES: {
        const std::vector<std::byte>& b{std::get<std::vector<std::byte>>(data_)};
        out.insert(out.end(), b.begin(), b.end());
        return;
    }
    }
    throw std::runtime_error("Value::serialize_to: type not found");
}

} // namespace duck