#include <gtest/gtest.h>
#include "mantisbase/core/context_store.h"
#include <string>

TEST(ContextStore, BasicOperations) {
    mb::ContextStore ctx;
    
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

TEST(ContextStore, HasKey) {
    mb::ContextStore ctx;
    
    EXPECT_FALSE(ctx.hasKey("nonexistent"));
    
    ctx.set<std::string>("exists", "value");
    EXPECT_TRUE(ctx.hasKey("exists"));
    EXPECT_FALSE(ctx.hasKey("nonexistent"));
}

TEST(ContextStore, GetOr) {
    mb::ContextStore ctx;
    
    // Get with default when key doesn't exist
    auto& val1 = ctx.getOr<std::string>("missing", "default");
    EXPECT_EQ(val1, "default");
    
    // Now the key should exist
    EXPECT_TRUE(ctx.hasKey("missing"));
    
    // Get existing value
    ctx.set<std::string>("exists", "actual");
    auto& val2 = ctx.getOr<std::string>("exists", "default");
    EXPECT_EQ(val2, "actual");
}

TEST(ContextStore, GetOrWithDifferentTypes) {
    mb::ContextStore ctx;
    
    auto& intVal = ctx.getOr<int>("int_key", 100);
    EXPECT_EQ(intVal, 100);
    
    auto& boolVal = ctx.getOr<bool>("bool_key", false);
    EXPECT_FALSE(boolVal);
    
    ctx.set<bool>("bool_key", true);
    auto& boolVal2 = ctx.getOr<bool>("bool_key", false);
    EXPECT_TRUE(boolVal2);
}

TEST(ContextStore, OverwriteValue) {
    mb::ContextStore ctx;
    
    ctx.set<std::string>("key", "original");
    auto val1 = ctx.get<std::string>("key");
    EXPECT_EQ(*val1.value(), "original");
    
    ctx.set<std::string>("key", "updated");
    auto val2 = ctx.get<std::string>("key");
    EXPECT_EQ(*val2.value(), "updated");
}

TEST(ContextStore, GetNonexistentKey) {
    mb::ContextStore ctx;
    
    auto val = ctx.get<std::string>("nonexistent");
    EXPECT_FALSE(val.has_value());
}

