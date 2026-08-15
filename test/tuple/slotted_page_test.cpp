#include "duck/config/sizes.hpp"
#include "duck/tuple/slotted_page.hpp"
#include "gtest/gtest.h"

class SlottedPageTest : public ::testing::Test {
protected:
    std::vector<std::byte> buffer = std::vector<std::byte>(duck::kPAGE_SIZE);

    duck::SlottedPage make_page() {
        duck::SlottedPage page((std::span(buffer)));
        page.init();
        return page;
    }
};

TEST_F(SlottedPageTest, InsertThenGetReturnsSameBytes) {
    auto page = make_page();
    std::string data = "hello world";
    auto bytes = std::as_bytes(std::span(data));

    auto rid = page.insert_tuple(bytes);
    ASSERT_TRUE(rid.has_value());

    auto result = page.get_tuple(rid->slot_num);
    ASSERT_EQ(result.size(), bytes.size());
    EXPECT_TRUE(std::equal(result.begin(), result.end(), bytes.begin()));
}

TEST_F(SlottedPageTest, DeleteThenGetReturnsEmpty) {
    auto page = make_page();
    std::string data = "temp";
    auto rid = page.insert_tuple(std::as_bytes(std::span(data)));
    ASSERT_TRUE(rid.has_value());

    EXPECT_TRUE(page.delete_tuple(rid->slot_num));
    EXPECT_TRUE(page.get_tuple(rid->slot_num).empty());
}

TEST_F(SlottedPageTest, DeletedSlotIsReusedByNextInsert) {
    auto page = make_page();
    std::string a = "aaaa";
    std::string b = "bb";

    auto rid_a = page.insert_tuple(std::as_bytes(std::span(a)));
    ASSERT_TRUE(rid_a.has_value());
    page.delete_tuple(rid_a->slot_num);

    auto rid_b = page.insert_tuple(std::as_bytes(std::span(b)));
    ASSERT_TRUE(rid_b.has_value());

    EXPECT_EQ(rid_b->slot_num, rid_a->slot_num); // reused slot, not a new one
}

TEST_F(SlottedPageTest, InsertFailsWhenPageFull) {
    auto page = make_page();
    std::vector<std::byte> big(duck::kPAGE_SIZE); // definitely won't fit

    auto rid = page.insert_tuple(big);
    EXPECT_FALSE(rid.has_value());
}

TEST_F(SlottedPageTest, MultipleInsertsGetCorrectData) {
    auto page = make_page();
    std::vector<std::string> values = {"first", "second", "third"};
    std::vector<duck::RID> rids;

    for (auto& v : values) {
        auto rid = page.insert_tuple(std::as_bytes(std::span(v)));
        ASSERT_TRUE(rid.has_value());
        rids.push_back(*rid);
    }

    for (size_t i = 0; i < values.size(); ++i) {
        auto result = page.get_tuple(rids[i].slot_num);
        ASSERT_EQ(result.size(), values[i].size());
        EXPECT_TRUE(std::equal(result.begin(), result.end(), std::as_bytes(std::span(values[i])).begin()));
    }
}