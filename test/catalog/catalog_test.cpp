/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/buffer/pool_manager.hpp"
#include "duck/catalog/catalog.hpp"
#include "duck/storage/disk_manager.hpp"
#include "duck/tuple/column.hpp"
#include "duck/tuple/schema.hpp"

#include <algorithm>
#include <cstdio>
#include <gtest/gtest.h>
#include <string_view>
#include <thread>
#include <vector>

class CatalogTest : public ::testing::Test {
protected:
    std::string test_file_ = "catalog_test.db";

    void SetUp() override {
        std::remove(test_file_.c_str());
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }

    static duck::Schema MakeUsersSchema() {
        return duck::Schema(std::vector<duck::Column>{
            {"id", duck::TypeId::UINT32},
            {"name", duck::TypeId::CHAR, 50},
            {"active", duck::TypeId::BOOL},
        });
    }
};

TEST_F(CatalogTest, CreateTableReturnsUsableTable) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::Catalog catalog{bpm, dm};

    duck::Table* table = catalog.create_table("users", MakeUsersSchema());
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(table->name(), "users");
    EXPECT_EQ(table->schema().column_count(), 3u);
}

TEST_F(CatalogTest, CreatingDuplicateTableThrows) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::Catalog catalog{bpm, dm};

    catalog.create_table("users", MakeUsersSchema());
    EXPECT_THROW(catalog.create_table("users", MakeUsersSchema()), std::runtime_error);
}

TEST_F(CatalogTest, GetTableReturnsNulloptWhenMissing) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::Catalog catalog{bpm, dm};

    EXPECT_FALSE(catalog.get_table("nonexistent").has_value());
}

TEST_F(CatalogTest, GetTableReturnsSameTableAfterCreate) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::Catalog catalog{bpm, dm};

    duck::Table* created = catalog.create_table("users", MakeUsersSchema());
    auto fetched = catalog.get_table("users");

    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched.value(), created); // same object, stable pointer
}

TEST_F(CatalogTest, CreateAndInsertRoundTrip) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::Catalog catalog{bpm, dm};

    duck::Table* table = catalog.create_table("users", MakeUsersSchema());

    duck::Tuple row(
        {
            duck::Value::of(static_cast<std::uint32_t>(1)),
            duck::Value::of(std::string("alice")),
            duck::Value::of(true),
        },
        table->schema());

    auto rid = table->insert_tuple(row);
    ASSERT_TRUE(rid.has_value());

    auto fetched = table->get_tuple(*rid);
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->get(0).as_uint32(), 1u);
    EXPECT_EQ(fetched->get(1).as_string(), "alice");
    EXPECT_EQ(fetched->get(2).as_bool(), true);
}

TEST_F(CatalogTest, AllTablesReturnsEveryCreatedTable) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::Catalog catalog{bpm, dm};

    catalog.create_table("users", MakeUsersSchema());
    catalog.create_table("cars", duck::Schema(std::vector<duck::Column>{
                                     {"id", duck::TypeId::UINT64},
                                     {"model", duck::TypeId::CHAR, 50},
                                 }));

    auto tables = catalog.all_tables();
    ASSERT_EQ(tables.size(), 2u);

    std::vector<std::string_view> names;
    for (auto* t : tables)
        names.push_back(t->name());
    std::sort(names.begin(), names.end());

    EXPECT_EQ(names[0], "cars");
    EXPECT_EQ(names[1], "users");
}

TEST_F(CatalogTest, PersistsAcrossReopen) {
    {
        duck::DiskManager dm{test_file_};
        duck::BufferPoolManager bpm{dm, 5};
        duck::Catalog catalog{bpm, dm};

        duck::Table* table = catalog.create_table("users", MakeUsersSchema());

        duck::Tuple row(
            {
                duck::Value::of(static_cast<std::uint32_t>(42)),
                duck::Value::of(std::string("bob")),
                duck::Value::of(false),
            },
            table->schema());
        table->insert_tuple(row);
    } // everything goes out of scope, BPM destructor flushes

    duck::DiskManager dm2{test_file_};
    duck::BufferPoolManager bpm2{dm2, 5};
    duck::Catalog catalog2{bpm2, dm2};

    auto fetched = catalog2.get_table("users");
    ASSERT_TRUE(fetched.has_value());

    duck::Table* table = fetched.value();
    EXPECT_EQ(table->schema().column_count(), 3u);

    auto scan = table->scan();
    auto entry = scan.next();
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->second.get(0).as_uint32(), 42u);
    EXPECT_EQ(entry->second.get(1).as_string(), "bob");
    EXPECT_EQ(entry->second.get(2).as_bool(), false);
}

TEST_F(CatalogTest, MultipleTablesPersistWithDistinctSchemas) {
    {
        duck::DiskManager dm{test_file_};
        duck::BufferPoolManager bpm{dm, 5};
        duck::Catalog catalog{bpm, dm};

        catalog.create_table("users", MakeUsersSchema());
        catalog.create_table("cars", duck::Schema(std::vector<duck::Column>{
                                         {"id", duck::TypeId::UINT64},
                                         {"model", duck::TypeId::CHAR, 50},
                                         {"year", duck::TypeId::INT32},
                                     }));
    }

    duck::DiskManager dm2{test_file_};
    duck::BufferPoolManager bpm2{dm2, 5};
    duck::Catalog catalog2{bpm2, dm2};

    auto users = catalog2.get_table("users");
    auto cars = catalog2.get_table("cars");

    ASSERT_TRUE(users.has_value());
    ASSERT_TRUE(cars.has_value());
    EXPECT_EQ(users.value()->schema().column_count(), 3u);
    EXPECT_EQ(cars.value()->schema().column_count(), 3u);
}

TEST_F(CatalogTest, ConcurrentCreateTableProducesUniqueTables) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 10};
    duck::Catalog catalog{bpm, dm};

    constexpr int kNumThreads = 8;
    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> succeeded(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&catalog, &succeeded, t]() {
            try {
                catalog.create_table("table_" + std::to_string(t), MakeUsersSchema());
                succeeded[t] = true;
            } catch (const std::exception&) {
                succeeded[t] = false;
            }
        });
    }

    for (auto& th : threads)
        th.join();

    for (bool ok : succeeded) {
        EXPECT_TRUE(ok);
    }
    EXPECT_EQ(catalog.all_tables().size(), static_cast<size_t>(kNumThreads));
}