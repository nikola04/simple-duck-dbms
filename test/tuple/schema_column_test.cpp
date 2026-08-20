/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/tuple/column.hpp"
#include "duck/tuple/schema.hpp"

#include <gtest/gtest.h>
#include <vector>

TEST(ColumnTest, RoundTripInt32Column) {
    duck::Column original("id", duck::TypeId::INT32);
    auto bytes = original.serialize();

    auto [restored, consumed] = duck::Column::deserialize(bytes);
    EXPECT_EQ(restored, original);
    EXPECT_EQ(consumed, bytes.size());
}

TEST(ColumnTest, RoundTripCharColumnWithLength) {
    duck::Column original("code", duck::TypeId::CHAR, 10);
    auto bytes = original.serialize();

    auto [restored, consumed] = duck::Column::deserialize(bytes);
    EXPECT_EQ(restored.name(), "code");
    EXPECT_EQ(restored.type(), duck::TypeId::CHAR);
    EXPECT_EQ(restored.length(), 10);
    EXPECT_EQ(consumed, bytes.size());
}

TEST(ColumnTest, ConsumedBytesAllowsSequentialParsing) {
    // Two columns serialized back-to-back must be parsed correctly using
    // the `consumed` byte count to advance the offset — this is exactly
    // how Schema::deserialize walks multiple columns.
    duck::Column first("a", duck::TypeId::INT32);
    duck::Column second("b", duck::TypeId::VARCHAR, 100);

    std::vector<std::byte> combined = first.serialize();
    auto second_bytes = second.serialize();
    combined.insert(combined.end(), second_bytes.begin(), second_bytes.end());

    auto [restored_first, offset1] = duck::Column::deserialize(combined);
    EXPECT_EQ(restored_first, first);

    auto [restored_second, offset2] = duck::Column::deserialize(std::span(combined).subspan(offset1));
    EXPECT_EQ(restored_second, second);
}

TEST(ColumnTest, DeserializeThrowsOnTruncatedData) {
    duck::Column original("name", duck::TypeId::VARCHAR, 50);
    auto bytes = original.serialize();

    // Chop off the last few bytes to simulate corrupted/incomplete data.
    std::vector<std::byte> truncated(bytes.begin(), bytes.end() - 2);

    EXPECT_THROW(duck::Column::deserialize(truncated), std::runtime_error);
}

TEST(SchemaTest, RoundTripMultipleColumns) {
    duck::Schema original({
        {"id", duck::TypeId::INT32, 0},
        {"name", duck::TypeId::VARCHAR, 100},
        {"active", duck::TypeId::BOOL, 0},
        {"code", duck::TypeId::CHAR, 8},
    });

    auto bytes = original.serialize();
    duck::Schema restored(bytes);

    ASSERT_EQ(restored.column_count(), original.column_count());
    for (size_t i = 0; i < original.column_count(); ++i) {
        EXPECT_EQ(restored.column(i), original.column(i));
    }
}

TEST(SchemaTest, RoundTripSingleColumn) {
    duck::Schema original({{"only_col", duck::TypeId::FLOAT, 0}});

    auto bytes = original.serialize();
    duck::Schema restored(bytes);

    ASSERT_EQ(restored.column_count(), 1u);
    EXPECT_EQ(restored.column(0), original.column(0));
}

TEST(SchemaTest, RoundTripPreservesColumnOrder) {
    duck::Schema original({
        {"third", duck::TypeId::INT32, 0},
        {"first", duck::TypeId::VARCHAR, 20},
        {"second", duck::TypeId::BOOL, 0},
    });

    auto bytes = original.serialize();
    duck::Schema restored(bytes);

    EXPECT_EQ(restored.column(0).name(), "third");
    EXPECT_EQ(restored.column(1).name(), "first");
    EXPECT_EQ(restored.column(2).name(), "second");
}

TEST(SchemaTest, CompatibleWithIdenticalSchema) {
    duck::Schema a({{"id", duck::TypeId::INT32, 0}, {"name", duck::TypeId::VARCHAR, 50}});
    duck::Schema b({{"id", duck::TypeId::INT32, 0}, {"name", duck::TypeId::VARCHAR, 50}});

    EXPECT_TRUE(a.compatible_with(b));
}

TEST(SchemaTest, NotCompatibleWithDifferentColumnCount) {
    duck::Schema a({{"id", duck::TypeId::INT32, 0}});
    duck::Schema b({{"id", duck::TypeId::INT32, 0}, {"name", duck::TypeId::VARCHAR, 50}});

    EXPECT_FALSE(a.compatible_with(b));
}

TEST(SchemaTest, NotCompatibleWithDifferentColumnOrder) {
    duck::Schema a({{"id", duck::TypeId::INT32, 0}, {"name", duck::TypeId::VARCHAR, 50}});
    duck::Schema b({{"name", duck::TypeId::VARCHAR, 50}, {"id", duck::TypeId::INT32, 0}});

    EXPECT_FALSE(a.compatible_with(b));
}

TEST(SchemaTest, NotCompatibleWithDifferentType) {
    duck::Schema a({{"value", duck::TypeId::INT32, 0}});
    duck::Schema b({{"value", duck::TypeId::FLOAT, 0}});

    EXPECT_FALSE(a.compatible_with(b));
}

TEST(SchemaTest, DeserializeThrowsOnTooSmallData) {
    std::vector<std::byte> tiny{std::byte{0}}; // only 1 byte, need at least 2 for column count

    EXPECT_THROW(duck::Schema{tiny}, std::runtime_error);
}

TEST(SchemaTest, FixedSizeOfMatchesExpectedWidths) {
    duck::Schema schema({
        {"a", duck::TypeId::INT32, 0},
        {"b", duck::TypeId::FLOAT, 0},
        {"c", duck::TypeId::BOOL, 0},
        {"d", duck::TypeId::CHAR, 15},
        {"e", duck::TypeId::VARCHAR, 200},
    });

    EXPECT_EQ(schema.fixed_size_of(0), 4);
    EXPECT_EQ(schema.fixed_size_of(1), 4);
    EXPECT_EQ(schema.fixed_size_of(2), 1);
    EXPECT_EQ(schema.fixed_size_of(3), 15);
    EXPECT_EQ(schema.fixed_size_of(4), 0);
}