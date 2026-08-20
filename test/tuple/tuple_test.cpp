/**
 * Copyright (c) 2026 Nikola Nedeljkovic
 * SPDX-License-Identifier: MIT
 */

#include "duck/tuple/schema.hpp"
#include "duck/tuple/tuple.hpp"
#include "duck/tuple/value.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace {

duck::Schema MakeFixedOnlySchema() {
    return duck::Schema({
        {"id", duck::TypeId::INT32, 0},
        {"active", duck::TypeId::BOOL, 0},
        {"score", duck::TypeId::FLOAT, 0},
    });
}

duck::Schema MakeMixedSchema() {
    return duck::Schema({
        {"id", duck::TypeId::INT32, 0},
        {"name", duck::TypeId::VARCHAR, 100},
        {"active", duck::TypeId::BOOL, 0},
    });
}

duck::Schema MakeCharSchema() {
    return duck::Schema({
        {"code", duck::TypeId::CHAR, 10},
    });
}

duck::Schema MakeMultiVarcharSchema() {
    return duck::Schema({
        {"a", duck::TypeId::VARCHAR, 50},
        {"b", duck::TypeId::VARCHAR, 50},
        {"c", duck::TypeId::VARCHAR, 50},
    });
}

duck::Schema MakeMultiInt32Schema() {
    return duck::Schema({
        {"a", duck::TypeId::INT32, 0},
        {"b", duck::TypeId::INT32, 0},
        {"c", duck::TypeId::INT32, 0},
    });
}

} // namespace

TEST(TupleTest, RoundTripFixedColumnsOnly) {
    duck::Schema schema = MakeFixedOnlySchema();

    std::vector<duck::Value> values{
        duck::Value::of(std::int32_t{42}),
        duck::Value::of(true),
        duck::Value::of(3.5f),
    };

    duck::Tuple original(values, schema);
    auto bytes = original.serialize();

    duck::Tuple restored(bytes, schema);
    EXPECT_EQ(restored.get(0).as_int32(), 42);
    EXPECT_EQ(restored.get(1).as_bool(), true);
    EXPECT_FLOAT_EQ(restored.get(2).as_float(), 3.5f);
}

TEST(TupleTest, RoundTripWithVarchar) {
    duck::Schema schema = MakeMixedSchema();

    std::vector<duck::Value> values{
        duck::Value::of(std::int32_t{7}),
        duck::Value::of(std::string("hello world")),
        duck::Value::of(false),
    };

    duck::Tuple original(values, schema);
    auto bytes = original.serialize();

    duck::Tuple restored(bytes, schema);
    EXPECT_EQ(restored.get(0).as_int32(), 7);
    EXPECT_EQ(restored.get(1).as_string(), "hello world");
    EXPECT_EQ(restored.get(2).as_bool(), false);
}

TEST(TupleTest, RoundTripMultipleVarcharColumns) {
    // Guards against the insert(begin(),...) vs insert(end(),...) class of bug —
    // three varlen columns in a row would reorder/corrupt if bytes are prepended.
    duck::Schema schema = MakeMultiVarcharSchema();

    std::vector<duck::Value> values{
        duck::Value::of(std::string("first")),
        duck::Value::of(std::string("second")),
        duck::Value::of(std::string("third")),
    };

    duck::Tuple original(values, schema);
    auto bytes = original.serialize();

    duck::Tuple restored(bytes, schema);
    EXPECT_EQ(restored.get(0).as_string(), "first");
    EXPECT_EQ(restored.get(1).as_string(), "second");
    EXPECT_EQ(restored.get(2).as_string(), "third");
}

TEST(TupleTest, ThreeConsecutiveInt32Columns) {
    duck::Schema schema = MakeMultiInt32Schema();

    std::vector<duck::Value> values{
        duck::Value::of(std::int32_t{1}),
        duck::Value::of(std::int32_t{2}),
        duck::Value::of(std::int32_t{3}),
    };

    duck::Tuple original(values, schema);
    auto bytes = original.serialize();

    duck::Tuple restored(bytes, schema);
    EXPECT_EQ(restored.get(0).as_int32(), 1);
    EXPECT_EQ(restored.get(1).as_int32(), 2);
    EXPECT_EQ(restored.get(2).as_int32(), 3);
}

TEST(TupleTest, NullFixedColumnRoundTrips) {
    duck::Schema schema = MakeFixedOnlySchema();

    std::vector<duck::Value> values{
        duck::Value::null(duck::ValueType::INT32),
        duck::Value::of(true),
        duck::Value::of(1.0f),
    };

    duck::Tuple original(values, schema);
    auto bytes = original.serialize();

    duck::Tuple restored(bytes, schema);
    EXPECT_TRUE(restored.get(0).is_null());
    EXPECT_FALSE(restored.get(1).is_null());
    EXPECT_EQ(restored.get(1).as_bool(), true);
}

TEST(TupleTest, NullVarcharColumnRoundTrips) {
    // Exercises the invariant that even a NULL varlen column must write a
    // 0-length prefix so offset computation for subsequent columns stays correct.
    duck::Schema schema = MakeMixedSchema();

    std::vector<duck::Value> values{
        duck::Value::of(std::int32_t{99}),
        duck::Value::null(duck::ValueType::STRING),
        duck::Value::of(true),
    };

    duck::Tuple original(values, schema);
    auto bytes = original.serialize();

    duck::Tuple restored(bytes, schema);
    EXPECT_EQ(restored.get(0).as_int32(), 99);
    EXPECT_TRUE(restored.get(1).is_null());
    EXPECT_EQ(restored.get(2).as_bool(), true); // column AFTER the null varlen must still read correctly
}

TEST(TupleTest, CharPaddingOnShortInput) {
    duck::Schema schema = MakeCharSchema();

    std::vector<duck::Value> values{duck::Value::of(std::string("abc"))};
    duck::Tuple original(values, schema);
    auto bytes = original.serialize();

    duck::Tuple restored(bytes, schema);
    EXPECT_EQ(restored.get(0).as_string(), "abc"); // trailing padding stripped on read
}

TEST(TupleTest, SerializeThrowsWhenCharValueTooLong) {
    duck::Schema schema = MakeCharSchema(); // CHAR(10)

    std::vector<duck::Value> values{duck::Value::of(std::string("this string is way too long"))};
    duck::Tuple tuple(values, schema);

    EXPECT_THROW(tuple.serialize(), std::runtime_error);
}

TEST(TupleTest, SerializeThrowsWhenVarcharValueTooLong) {
    duck::Schema schema = MakeMixedSchema(); // name is VARCHAR(100)

    std::vector<duck::Value> values{
        duck::Value::of(std::int32_t{1}),
        duck::Value::of(std::string(150, 'x')), // exceeds max length of 100
        duck::Value::of(true),
    };
    duck::Tuple tuple(values, schema);

    EXPECT_THROW(tuple.serialize(), std::runtime_error);
}

TEST(TupleTest, ConstructorThrowsOnValueCountMismatch) {
    duck::Schema schema = MakeFixedOnlySchema(); // expects 3 columns

    std::vector<duck::Value> values{
        duck::Value::of(std::int32_t{1}),
        duck::Value::of(true),
        // missing the third value
    };

    EXPECT_THROW(duck::Tuple(values, schema), std::runtime_error);
}

TEST(TupleTest, ConstructorThrowsOnTypeMismatch) {
    duck::Schema schema = MakeFixedOnlySchema(); // column 0 is INT32

    std::vector<duck::Value> values{
        duck::Value::of(std::string("not an int")), // wrong type for column 0
        duck::Value::of(true),
        duck::Value::of(1.0f),
    };

    EXPECT_THROW(duck::Tuple(values, schema), std::runtime_error);
}