#pragma once

#include "duck/common/rid.hpp"
#include "duck/table/table_heap.hpp"
#include "duck/tuple/schema.hpp"
#include "duck/tuple/tuple.hpp"
#include <optional>
#include <utility>

namespace duck {

class Table {
public:
    explicit Table(TableHeap& table_heap, const Schema& schema);

    std::optional<RID> insert_tuple(Tuple& tuple);
    std::optional<Tuple> get_tuple(RID rid) const;
    bool delete_tuple(RID rid);

    class Scan;
    Scan scan() const;

private:
    TableHeap& table_heap_;
    const Schema& schema_;
};

class Table::Scan {
public:
    Scan(TableHeap::Scan heap_scan, const Schema& schema) : heap_scan_(std::move(heap_scan)), schema_(schema) {
    }

    std::optional<std::pair<RID, Tuple>> next();

private:
    TableHeap::Scan heap_scan_;
    const Schema& schema_;
};

} // namespace duck