#include <gtest/gtest.h>
#include "mantisbase/core/context_store.h"
#include <string>

TEST(MbContextStore, BasicOperations) {
    mb::MbContextStore ctx;
    
    // Test set and get
    ctx.set<std::string>("key1", "value1");
    ctx.set<int>("key2", 42);
    ctx.set<bool>("key3", true);
    
    auto val1 = ctx.get<std::string>("key1");
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1.value(), "value1");
    
    auto val2 = ctx.get<int>("key2");
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2.value(), 42);
    
    auto val3 = ctx.get<bool>("key3");
    EXPECT_TRUE(val3.has_value());
    EXPECT_EQ(*val3.value(), true);
}

TEST(MbContextStore, HasKey) {
    mb::MbContextStore ctx;
    
    EXPECT_FALSE(ctx.hasKey("nonexistent"));
    
    ctx.set<std::string>("exists", "value");
    EXPECT_TRUE(ctx.hasKey("exists"));
    EXPECT_FALSE(ctx.hasKey("nonexistent"));
}

TEST(MbContextStore, GetOr) {
    mb::MbContextStore ctx;
    
    // Get with default when key doesn't exist
    auto& val1 = ctx.get<std::string>("missing", "default");
    EXPECT_EQ(val1, "default");
    
    // Now the key should exist
    EXPECT_TRUE(ctx.hasKey("missing"));
    
    // Get existing value
    ctx.set<std::string>("exists", "actual");
    auto& val2 = ctx.get<std::string>("exists", "default");
    EXPECT_EQ(val2, "actual");
}

TEST(MbContextStore, GetOrWithDifferentTypes) {
    mb::MbContextStore ctx;
    
    auto& intVal = ctx.get<int>("int_key", 100);
    EXPECT_EQ(intVal, 100);
    
    auto& boolVal = ctx.get<bool>("bool_key", false);
    EXPECT_FALSE(boolVal);
    
    ctx.set<bool>("bool_key", true);
    auto& boolVal2 = ctx.get<bool>("bool_key", false);
    EXPECT_TRUE(boolVal2);
}

TEST(MbContextStore, OverwriteValue) {
    mb::MbContextStore ctx;
    
    ctx.set<std::string>("key", "original");
    auto val1 = ctx.get<std::string>("key");
    EXPECT_EQ(*val1.value(), "original");
    
    ctx.set<std::string>("key", "updated");
    auto val2 = ctx.get<std::string>("key");
    EXPECT_EQ(*val2.value(), "updated");
}

TEST(MbContextStore, GetNonexistentKey) {
    mb::MbContextStore ctx;
    
    auto val = ctx.get<std::string>("nonexistent");
    EXPECT_FALSE(val.has_value());
}

