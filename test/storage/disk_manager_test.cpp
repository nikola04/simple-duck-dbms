#include "duck/config/sizes.hpp"
#include "duck/storage/disk_manager.hpp"
#include "thread"
#include "unordered_set"
#include "gtest/gtest.h"
#include <cstddef>

class DiskManagerTest : public ::testing::Test {
protected:
    std::string test_file_ = "disk_manager_test.db";

    void SetUp() override {
        std::remove(test_file_.c_str());
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }
};

TEST_F(DiskManagerTest, WriteReadRoundTrip) {
    duck::DiskManager dm{test_file_};

    size_t page_id = dm.allocate_page();

    std::byte write_buf[duck::kPAGE_SIZE] = {};
    std::memcpy(write_buf, "hello page", 10);

    dm.write_page(page_id, write_buf);

    std::byte read_buf[duck::kPAGE_SIZE] = {};
    dm.read_page(page_id, read_buf);

    EXPECT_EQ(std::memcmp(write_buf, read_buf, duck::kPAGE_SIZE), 0);
}

TEST_F(DiskManagerTest, FreshPageReadsAsZero) {
    duck::DiskManager dm{test_file_};

    size_t page_id = dm.allocate_page();

    std::byte read_buf[duck::kPAGE_SIZE];
    std::memset(read_buf, 0xFF, duck::kPAGE_SIZE);

    dm.read_page(page_id, read_buf);

    char zeros[duck::kPAGE_SIZE] = {};
    EXPECT_EQ(std::memcmp(read_buf, zeros, duck::kPAGE_SIZE), 0);
}

TEST_F(DiskManagerTest, MultiplePagesDontOverlap) {
    duck::DiskManager dm{test_file_};

    size_t p0 = dm.allocate_page();
    size_t p1 = dm.allocate_page();
    size_t p2 = dm.allocate_page();

    std::byte buf0_[duck::kPAGE_SIZE] = {}, buf1_[duck::kPAGE_SIZE] = {}, buf2_[duck::kPAGE_SIZE] = {};
    std::span<std::byte> buf0 = {buf0_}, buf1 = {buf1_}, buf2 = {buf2_};
    std::memset(buf0_, 'A', duck::kPAGE_SIZE);
    std::memset(buf1_, 'B', duck::kPAGE_SIZE);
    std::memset(buf2_, 'C', duck::kPAGE_SIZE);

    dm.write_page(p0, buf0);
    dm.write_page(p1, buf1);
    dm.write_page(p2, buf2);

    std::byte read0[duck::kPAGE_SIZE], read1[duck::kPAGE_SIZE], read2[duck::kPAGE_SIZE];
    dm.read_page(p0, std::span<std::byte>{read0});
    dm.read_page(p1, std::span<std::byte>{read1});
    dm.read_page(p2, std::span<std::byte>{read2});

    EXPECT_EQ(std::memcmp(read0, buf0_, duck::kPAGE_SIZE), 0);
    EXPECT_EQ(std::memcmp(read1, buf1_, duck::kPAGE_SIZE), 0);
    EXPECT_EQ(std::memcmp(read2, buf2_, duck::kPAGE_SIZE), 0);
}

TEST_F(DiskManagerTest, CapacityGrowsWithAllocation) {
    duck::DiskManager dm{test_file_};

    EXPECT_EQ(dm.capacity(), 0u);
    dm.allocate_page();
    EXPECT_EQ(dm.capacity(), 1u);
    dm.allocate_page();
    EXPECT_EQ(dm.capacity(), 2u);
}

TEST_F(DiskManagerTest, ReadWriteOutOfBoundsThrows) {
    duck::DiskManager dm{test_file_};

    dm.allocate_page();

    std::byte buf[duck::kPAGE_SIZE] = {};
    EXPECT_THROW(dm.read_page(5, buf), std::runtime_error);
    EXPECT_THROW(dm.write_page(5, buf), std::runtime_error);
}

TEST_F(DiskManagerTest, DeallocatedPageIsReused) {
    duck::DiskManager dm{test_file_};

    size_t p0 = dm.allocate_page();
    dm.deallocate_page(p0);

    size_t p1 = dm.allocate_page();

    EXPECT_EQ(p0, p1);
    EXPECT_EQ(dm.capacity(), 1u);
}

TEST_F(DiskManagerTest, PersistsAcrossReopen) {
    {
        duck::DiskManager dm{test_file_};
        size_t page_id = dm.allocate_page();

        std::byte buf[duck::kPAGE_SIZE] = {};
        std::memcpy(buf, "persisted", 9);
        dm.write_page(page_id, buf);
    }
    duck::DiskManager dm2{test_file_};
    EXPECT_EQ(dm2.capacity(), 1u);

    std::byte read_buf[duck::kPAGE_SIZE] = {};
    dm2.read_page(0, read_buf);
    EXPECT_EQ(std::memcmp(read_buf, "persisted", 9), 0);
}

TEST_F(DiskManagerTest, ConcurrentAllocateProducesUniqueIds) {
    duck::DiskManager dm{test_file_};

    constexpr int kNumThreads = 8;
    constexpr int kAllocationsPerThread = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<size_t>> results(kNumThreads);

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&dm, &results, t]() {
            for (int i = 0; i < kAllocationsPerThread; ++i) {
                results[t].push_back(dm.allocate_page());
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    std::unordered_set<size_t> seen;
    for (auto& thread_results : results) {
        for (size_t id : thread_results) {
            auto [it, inserted] = seen.insert(id);
            EXPECT_TRUE(inserted) << "Duplicate page_id allocated: " << id;
        }
    }

    EXPECT_EQ(seen.size(), static_cast<size_t>(kNumThreads * kAllocationsPerThread));
}