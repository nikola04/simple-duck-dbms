#include "duck/tuple/schema.hpp"
#include <algorithm>
#include <cstddef>
#include <string>

namespace duck {

std::uint16_t Schema::fixed_size_of(size_t index) const {
    switch (const Column& column{this->column(index)}; column.type) {
    case TypeId::BOOL:
        return 1;
    case TypeId::FLOAT:
        return 4;
    case TypeId::INT32:
        return 4;
    case TypeId::CHAR:
        return column.length;
    case TypeId::VARCHAR:
        return 0;
    }
}

bool Schema::compatible_with(const Schema& other) const {
    if (this == &other)
        return true;

    return std::ranges::equal(columns_, other.columns_);
}

std::string Schema::to_string() const {
    std::string s;
    size_t n{columns_.size()};
    s.reserve(n * 4);

    for (size_t i{0}; i < n; ++i) {
        switch (columns_[i].type) {
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
            s += "CHAR(" + std::to_string(columns_[i].length) + ")";
            break;
        }
        if (i != n - 1)
            s += " | ";
    }

    return s;
}

} // namespace duck