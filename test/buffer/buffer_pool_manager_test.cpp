/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/buffer/pool_manager.hpp"
#include "duck/storage/disk_manager.hpp"

#include <cstdio>
#include <cstring>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

class BufferPoolManagerTest : public ::testing::Test {
protected:
    std::string test_file_ = "bpm_test.db";

    void SetUp() override {
        std::remove(test_file_.c_str());
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }
};

TEST_F(BufferPoolManagerTest, NewPageReturnsValidPointer) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* page = bpm.new_page();
    ASSERT_NE(page, nullptr);
    EXPECT_EQ(page->pin_count(), 1u);

    bpm.unpin_page(page->page_id());
}

TEST_F(BufferPoolManagerTest, NewPageIsZeroed) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* page = bpm.new_page();
    ASSERT_NE(page, nullptr);

    char zeros[duck::kPAGE_SIZE] = {};
    EXPECT_EQ(std::memcmp(page->data(), zeros, duck::kPAGE_SIZE), 0);

    bpm.unpin_page(page->page_id());
}

TEST_F(BufferPoolManagerTest, WriteThenFetchSeesSameData) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* page = bpm.new_page();
    duck::PageID id = page->page_id();
    std::memcpy(page->data(), "hello", 5);
    bpm.unpin_page(id, /*dirty=*/true);

    duck::Page* fetched = bpm.fetch_page(id);
    ASSERT_NE(fetched, nullptr);
    EXPECT_EQ(fetched, page);
    EXPECT_EQ(std::memcmp(fetched->data(), "hello", 5), 0);

    bpm.unpin_page(id);
}

TEST_F(BufferPoolManagerTest, PinCountIncrementsOnRepeatedFetch) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* page = bpm.new_page();
    duck::PageID id = page->page_id();
    EXPECT_EQ(page->pin_count(), 1u);

    duck::Page* fetched = bpm.fetch_page(id);
    EXPECT_EQ(fetched->pin_count(), 2u);

    bpm.unpin_page(id);
    EXPECT_EQ(page->pin_count(), 1u);
    bpm.unpin_page(id);
    EXPECT_EQ(page->pin_count(), 0u);
}

TEST_F(BufferPoolManagerTest, EvictsUnpinnedPageWhenPoolFull) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 2};

    duck::Page* p0 = bpm.new_page();
    duck::PageID id0 = p0->page_id();
    bpm.unpin_page(id0);

    duck::Page* p1 = bpm.new_page();
    duck::PageID id1 = p1->page_id();
    bpm.unpin_page(id1);

    duck::Page* p2 = bpm.new_page();
    ASSERT_NE(p2, nullptr);
    duck::PageID id2 = p2->page_id();
    bpm.unpin_page(id2);

    duck::Page* refetched0 = bpm.fetch_page(id0);
    ASSERT_NE(refetched0, nullptr);
    EXPECT_EQ(refetched0->page_id(), id0);
    bpm.unpin_page(id0);
}

TEST_F(BufferPoolManagerTest, PinnedPageIsNotEvicted) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 1};

    duck::Page* p0 = bpm.new_page();
    duck::PageID id0 = p0->page_id();

    duck::Page* p1 = bpm.new_page();
    EXPECT_EQ(p1, nullptr);

    bpm.unpin_page(id0);
}

TEST_F(BufferPoolManagerTest, DirtyPageIsFlushedOnEviction) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 1};

    duck::Page* p0 = bpm.new_page();
    duck::PageID id0 = p0->page_id();
    std::memcpy(p0->data(), "persisted-data", 14);
    bpm.unpin_page(id0, /*dirty=*/true);

    duck::Page* p1 = bpm.new_page();
    ASSERT_NE(p1, nullptr);
    bpm.unpin_page(p1->page_id());

    duck::Page* refetched = bpm.fetch_page(id0);
    ASSERT_NE(refetched, nullptr);
    EXPECT_EQ(std::memcmp(refetched->data(), "persisted-data", 14), 0);
    bpm.unpin_page(id0);
}

TEST_F(BufferPoolManagerTest, ConcurrentFetchUnpinNoDataRace) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 4};

    constexpr int kNumPages = 8;
    constexpr int kNumThreads = 8;
    constexpr int kIterations = 200;

    std::vector<duck::PageID> page_ids;
    for (int i = 0; i < kNumPages; ++i) {
        duck::Page* page = bpm.new_page();
        ASSERT_NE(page, nullptr);
        page_ids.push_back(page->page_id());
        bpm.unpin_page(page->page_id());
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&bpm, &page_ids, t]() {
            for (int i = 0; i < kIterations; ++i) {
                duck::PageID id = page_ids[(t + i) % page_ids.size()];
                duck::Page* page = bpm.fetch_page(id);
                if (page == nullptr) {
                    continue;
                }
                {
                    std::unique_lock lock(page->latch());
                    std::memset(page->data(), static_cast<char>(t), duck::kPAGE_SIZE);
                }
                bpm.unpin_page(id, /*dirty=*/true);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    SUCCEED();
}

TEST_F(BufferPoolManagerTest, UnpinNonCachedPageDoesNotCrash) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    EXPECT_NO_THROW(bpm.unpin_page(999, /*dirty=*/true));
}

TEST_F(BufferPoolManagerTest, FlushAllPersistsDirtyPages) {
    duck::PageID id0, id1;

    {
        duck::DiskManager dm{test_file_};
        duck::BufferPoolManager bpm{dm, 3};

        duck::Page* p0 = bpm.new_page();
        id0 = p0->page_id();
        std::memcpy(p0->data(), "page-zero", 9);
        bpm.unpin_page(id0, /*dirty=*/true);

        duck::Page* p1 = bpm.new_page();
        id1 = p1->page_id();
        std::memcpy(p1->data(), "page-one", 8);
        bpm.unpin_page(id1, /*dirty=*/true);
    }

    duck::DiskManager dm2{test_file_};
    duck::BufferPoolManager bpm2{dm2, 3};

    duck::Page* r0 = bpm2.fetch_page(id0);
    ASSERT_NE(r0, nullptr);
    EXPECT_EQ(std::memcmp(r0->data(), "page-zero", 9), 0);
    bpm2.unpin_page(id0);

    duck::Page* r1 = bpm2.fetch_page(id1);
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(std::memcmp(r1->data(), "page-one", 8), 0);
    bpm2.unpin_page(id1);
}

TEST_F(BufferPoolManagerTest, CleanPageIsNotFlushedOnEviction) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 1};

    duck::Page* p0 = bpm.new_page();
    duck::PageID id0 = p0->page_id();
    std::memcpy(p0->data(), "should-not-persist", 19);
    bpm.unpin_page(id0, /*dirty=*/false);

    duck::Page* p1 = bpm.new_page();
    ASSERT_NE(p1, nullptr);
    bpm.unpin_page(p1->page_id());

    char raw[duck::kPAGE_SIZE];
    dm.read_page(id0, raw);
    EXPECT_NE(std::memcmp(raw, "should-not-persist", 19), 0);
}

TEST_F(BufferPoolManagerTest, StressMixedDirtyEvictionUnderPressure) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    constexpr int kNumPages = 20;
    std::vector<duck::PageID> ids;

    for (int i = 0; i < kNumPages; ++i) {
        duck::Page* page = bpm.new_page();
        ASSERT_NE(page, nullptr);
        duck::PageID id = page->page_id();
        std::memcpy(page->data(), &id, sizeof(id));
        bpm.unpin_page(id, /*dirty=*/true);
        ids.push_back(id);
    }

    for (duck::PageID id : ids) {
        duck::Page* page = bpm.fetch_page(id);
        ASSERT_NE(page, nullptr) << "fetch failed for page " << id;
        duck::PageID stored;
        std::memcpy(&stored, page->data(), sizeof(stored));
        EXPECT_EQ(stored, id) << "content mismatch for page " << id;
        bpm.unpin_page(id);
    }
}

TEST_F(BufferPoolManagerTest, DeleteUnpinnedPageSucceeds) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* page = bpm.new_page();
    duck::PageID id = page->page_id();
    bpm.unpin_page(id);

    EXPECT_TRUE(bpm.delete_page(id));
}

TEST_F(BufferPoolManagerTest, DeletePinnedPageFails) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* page = bpm.new_page();
    duck::PageID id = page->page_id();
    // intentionally NOT unpinned — page is still in use

    EXPECT_FALSE(bpm.delete_page(id));

    bpm.unpin_page(id);
}

TEST_F(BufferPoolManagerTest, DeletedFrameIsReusedBeforeEviction) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 1}; // single frame pool

    duck::Page* p0 = bpm.new_page();
    duck::PageID id0 = p0->page_id();
    bpm.unpin_page(id0);

    ASSERT_TRUE(bpm.delete_page(id0));

    // Pool has only one frame; it was freed by delete_page.
    // A new page should be able to use it directly, without needing an eviction victim.
    duck::Page* p1 = bpm.new_page();
    ASSERT_NE(p1, nullptr);
    bpm.unpin_page(p1->page_id());
}

TEST_F(BufferPoolManagerTest, DeletedPageIdIsRecycledByDiskManager) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* p0 = bpm.new_page();
    duck::PageID id0 = p0->page_id();
    bpm.unpin_page(id0);

    ASSERT_TRUE(bpm.delete_page(id0));

    // DiskManager's free-list should hand back the same page_id on next allocation.
    duck::Page* p1 = bpm.new_page();
    EXPECT_EQ(p1->page_id(), id0);
    bpm.unpin_page(p1->page_id());
}

TEST_F(BufferPoolManagerTest, DeleteNonCachedPageStillDeallocatesOnDisk) {
    duck::DiskManager dm{test_file_};
    duck::BufferPoolManager bpm{dm, 3};

    duck::Page* p0 = bpm.new_page();
    duck::PageID id0 = p0->page_id();
    bpm.unpin_page(id0);

    // Evict it out of the cache by filling the pool with other pages,
    // so id0 is no longer present in page_table_ when we delete it.
    for (int i = 0; i < 5; ++i) {
        duck::Page* p = bpm.new_page();
        ASSERT_NE(p, nullptr);
        bpm.unpin_page(p->page_id());
    }

    EXPECT_TRUE(bpm.delete_page(id0));

    // page_id should be recyclable afterwards
    duck::Page* recycled = bpm.new_page();
    EXPECT_EQ(recycled->page_id(), id0);
    bpm.unpin_page(recycled->page_id());
}