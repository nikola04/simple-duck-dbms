#include "duck/tuple/schema.hpp"
#include <cstddef>

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

} // namespace duck