#include "duck/catalog/catalog.hpp"
#include "duck/buffer/pool_manager.hpp"
#include "duck/common/types.hpp"
#include "duck/config/defaults.hpp"
#include "duck/table/table.hpp"
#include "duck/table/table_heap.hpp"
#include "duck/tuple/column.hpp"
#include "duck/tuple/schema.hpp"
#include "duck/tuple/value.hpp"
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <vector>

namespace duck {

const Schema Catalog::catalog_schema_{std::vector<Column>{
    {"table_name", TypeId::VARCHAR, 128},
    {"first_page_id", TypeId::UINT32},
    {"schema_bytes", TypeId::VARBINARY, 4096},
}};

Catalog::Catalog(BufferPoolManager& bpm, DiskManager& disk_manager)
    : bpm_(bpm), disk_manager_(disk_manager), catalog_table_(init_table()) {
    load_existing_tables();
}

Table Catalog::init_table() {
    if (disk_manager_.capacity() == 0) {
        return Table{"duck_catalog_", duck::TableHeap::create(bpm_), catalog_schema_};
    }
    TableHeap heap{kCATALOG_FIRST_PAGE_ID, bpm_};
    return Table{"duck_catalog_", std::move(heap), catalog_schema_};
}

Table* Catalog::create_table(const std::string name, Schema schema) {
    std::unique_lock<std::shared_mutex> lock{latch_};
    if (auto it{tables_.find(name)}; it != tables_.end())
        throw std::runtime_error(std::format("Catalog::create_table: table with name {} already exists", name));

    TableHeap heap{TableHeap::create(bpm_)};

    Tuple entry({std::vector<Value>{Value::of(name), Value::of(heap.first_page_id()), Value::of(schema.serialize())},
                 catalog_schema_});

    if (!catalog_table_.insert_tuple(entry).has_value()) {
        throw std::runtime_error("Catalog::CreateTable: failed to write catalog entry");
    }

    auto schema_ptr{std::make_unique<Schema>(schema)};
    auto table_ptr{std::make_unique<Table>(name, std::move(heap), *schema_ptr)};

    Table* result = table_ptr.get();
    tables_[name] = TableEntry{std::move(schema_ptr), std::move(table_ptr)};

    return result;
}

std::optional<Table*> Catalog::get_table(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock{latch_};
    if (auto it{tables_.find(name)}; it != tables_.end()) {
        return it->second.table.get();
    }
    return std::nullopt;
}

std::vector<Table*> Catalog::all_tables() const {
    std::shared_lock<std::shared_mutex> lock{latch_};

    std::vector<Table*> tables;
    tables.reserve(tables_.size());

    for (auto& e : tables_)
        tables.push_back(e.second.table.get());

    return tables;
}

void Catalog::load_existing_tables() {
    std::unique_lock<std::shared_mutex> lock{latch_};

    Table::Scan scan{catalog_table_.scan()};
    while (auto entry{scan.next()}) {
        const duck::Tuple& tuple{entry.value().second};

        std::string table_name{tuple.get(0).as_string()};
        PageID first_page_id{tuple.get(1).as_uint32()};

        auto schema{std::make_unique<Schema>(tuple.get(2).as_bytes())};
        auto table_ptr{std::make_unique<Table>(table_name, TableHeap{first_page_id, bpm_}, *schema)};

        tables_[table_name] = TableEntry{std::move(schema), std::move(table_ptr)};
    }
}

} // namespace duck