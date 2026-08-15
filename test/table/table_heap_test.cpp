/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"
#include "duck/table/table_heap.hpp"

#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>

class TableHeapTest : public ::testing::Test {
protected:
    std::string test_file_ = "table_heap_test.db";

    void SetUp() override {
        std::remove(test_file_.c_str());
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }

    static std::span<const std::byte> as_bytes(const std::string& s) {
        return std::as_bytes(std::span(s));
    }
};

TEST_F(TableHeapTest, InsertThenGetReturnsSameData) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::TableHeap heap = duck::TableHeap::create(bpm);

    std::string value = "hello table heap";
    auto rid = heap.insert_tuple(as_bytes(value));
    ASSERT_TRUE(rid.has_value());

    auto result = heap.get_tuple(*rid);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), value.size());
    EXPECT_TRUE(std::equal(result->begin(), result->end(), as_bytes(value).begin()));
}

TEST_F(TableHeapTest, DeleteThenGetReturnsNullopt) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::TableHeap heap = duck::TableHeap::create(bpm);

    std::string value = "temporary";
    auto rid = heap.insert_tuple(as_bytes(value));
    ASSERT_TRUE(rid.has_value());

    EXPECT_TRUE(heap.delete_tuple(*rid));
    EXPECT_FALSE(heap.get_tuple(*rid).has_value());
}

TEST_F(TableHeapTest, DeletingTwiceFails) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::TableHeap heap = duck::TableHeap::create(bpm);

    std::string value = "once";
    auto rid = heap.insert_tuple(as_bytes(value));
    ASSERT_TRUE(rid.has_value());

    EXPECT_TRUE(heap.delete_tuple(*rid));
    EXPECT_FALSE(heap.delete_tuple(*rid)); // already deleted
}

TEST_F(TableHeapTest, GetTupleWithInvalidSlotReturnsNullopt) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::TableHeap heap = duck::TableHeap::create(bpm);

    std::string value = "exists";
    auto rid = heap.insert_tuple(as_bytes(value));
    ASSERT_TRUE(rid.has_value());

    // Same page, but a slot number that was never inserted into
    duck::RID bogus_slot{rid->page_id, static_cast<std::uint16_t>(rid->slot_num + 50)};
    EXPECT_FALSE(heap.get_tuple(bogus_slot).has_value());
}

TEST_F(TableHeapTest, MultipleInsertsPreserveIndividualData) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 5};
    duck::TableHeap heap = duck::TableHeap::create(bpm);

    std::vector<std::string> values = {"alpha", "beta", "gamma", "delta"};
    std::vector<duck::RID> rids;

    for (auto& v : values) {
        auto rid = heap.insert_tuple(as_bytes(v));
        ASSERT_TRUE(rid.has_value());
        rids.push_back(*rid);
    }

    for (size_t i = 0; i < values.size(); ++i) {
        auto result = heap.get_tuple(rids[i]);
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result->size(), values[i].size());
        EXPECT_TRUE(std::equal(result->begin(), result->end(), as_bytes(values[i]).begin()));
    }
}

TEST_F(TableHeapTest, InsertSpillsOntoNewPageWhenFull) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 10}; // enough frames to hold a handful of pages
    duck::TableHeap heap = duck::TableHeap::create(bpm);

    // Insert tuples large enough that a single page cannot hold many of them,
    // forcing TableHeap to allocate and link additional pages.
    std::string big_value(500, 'x');
    std::vector<duck::RID> rids;

    constexpr int kNumTuples = 20; // 20 * 500 bytes >> one 4KB page
    for (int i = 0; i < kNumTuples; ++i) {
        auto rid = heap.insert_tuple(as_bytes(big_value));
        ASSERT_TRUE(rid.has_value()) << "insert failed at index " << i;
        rids.push_back(*rid);
    }

    // Verify tuples landed on more than one distinct page.
    std::set<duck::PageID> distinct_pages;
    for (auto& rid : rids) {
        distinct_pages.insert(rid.page_id);
    }
    EXPECT_GT(distinct_pages.size(), 1u);

    // All tuples must still be retrievable, regardless of which page they ended up on.
    for (auto& rid : rids) {
        auto result = heap.get_tuple(rid);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->size(), big_value.size());
    }
}

TEST_F(TableHeapTest, ConcurrentInsertsAllSucceedWithUniqueRids) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 10};
    duck::TableHeap heap = duck::TableHeap::create(bpm);

    constexpr int kNumThreads = 8;
    constexpr int kInsertsPerThread = 30;
    std::string value(200, 'y'); // forces multiple pages under concurrent load

    std::vector<std::thread> threads;
    std::vector<std::vector<duck::RID>> results(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&heap, &results, &value, t]() {
            for (int i = 0; i < kInsertsPerThread; ++i) {
                auto rid = heap.insert_tuple(std::as_bytes(std::span(value)));
                if (rid.has_value()) {
                    results[t].push_back(*rid);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Every RID across all threads must be unique — no two inserts should have
    // landed on the same {page_id, slot_num}.
    std::set<std::pair<duck::PageID, std::uint16_t>> seen;
    size_t total_inserted = 0;
    for (auto& thread_rids : results) {
        for (auto& rid : thread_rids) {
            auto [it, inserted] = seen.insert({rid.page_id, rid.slot_num});
            EXPECT_TRUE(inserted) << "Duplicate RID: page=" << rid.page_id << " slot=" << rid.slot_num;
            ++total_inserted;
        }
    }

    EXPECT_EQ(total_inserted, static_cast<size_t>(kNumThreads * kInsertsPerThread));

    // Sanity check: every inserted tuple is still readable and has the right content.
    for (auto& thread_rids : results) {
        for (auto& rid : thread_rids) {
            auto result = heap.get_tuple(rid);
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result->size(), value.size());
        }
    }
}