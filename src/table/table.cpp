#include "duck/table/table.hpp"
#include "duck/tuple/tuple.hpp"
#include <optional>
#include <stdexcept>

namespace duck {

Table::Table(TableHeap& table_heap, const Schema& schema) : table_heap_(table_heap), schema_(schema) {
}

std::optional<RID> Table::insert_tuple(const Tuple& tuple) {
    if (!schema_.compatible_with(tuple.schema()))
        throw std::runtime_error("Table::insert_tuple: schemas not compatible");

    return table_heap_.insert_tuple(tuple.serialize());
}

std::optional<Tuple> Table::get_tuple(RID rid) const {
    auto bytes{table_heap_.get_tuple(rid)};
    if (!bytes.has_value())
        return std::nullopt;

    return Tuple{bytes.value(), schema_};
}

bool Table::delete_tuple(RID rid) {
    return table_heap_.delete_tuple(rid);
}

Table::Scan Table::scan() const {
    return Table::Scan{table_heap_.scan(), schema_};
}

std::optional<std::pair<RID, Tuple>> Table::Scan::next() {
    auto result{heap_scan_.next()};
    if (!result.has_value())
        return std::nullopt;

    return std::pair{result->first, Tuple{result->second, schema_}};
}

} // namespace duck