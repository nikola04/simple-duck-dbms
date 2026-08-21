#pragma once

#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"
#include "duck/table/table.hpp"
#include "duck/tuple/schema.hpp"
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace duck {

struct TableEntry {
    std::unique_ptr<Schema> schema;
    std::unique_ptr<Table> table;
};

class Catalog {
public:
    Catalog(BufferPoolManager& bpm, DiskManager& disk_manager);

    Table* create_table(const std::string name, Schema schema);
    std::optional<Table*> get_table(const std::string& name);

    std::vector<Table*> all_tables() const;

private:
    static const Schema catalog_schema_;
    BufferPoolManager& bpm_;
    DiskManager& disk_manager_;

    Table catalog_table_;
    mutable std::shared_mutex latch_;

    std::unordered_map<std::string, TableEntry> tables_;

    Table init_table();
    void load_existing_tables();
};

} // namespace duck