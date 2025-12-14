#include <gtest/gtest.h>
#include "mantisbase/core/models/access_rules.h"
#include <nlohmann/json.hpp>

TEST(AccessRule, DefaultConstructor) {
    mb::AccessRule rule;
    
    EXPECT_EQ(rule.mode(), "");
    EXPECT_EQ(rule.expr(), "");
}

TEST(AccessRule, ConstructorWithParams) {
    mb::AccessRule rule("custom", "auth.id != \"\"");
    
    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "auth.id != \"\"");
}

TEST(AccessRule, SetModeAndExpr) {
    mb::AccessRule rule;
    
    rule.setMode("public");
    rule.setExpr("");
    
    EXPECT_EQ(rule.mode(), "public");
    EXPECT_EQ(rule.expr(), "");
    
    rule.setMode("auth");
    EXPECT_EQ(rule.mode(), "auth");
    
    rule.setMode("custom");
    rule.setExpr("auth.entity == \"mb_admins\"");
    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "auth.entity == \"mb_admins\"");
}

TEST(AccessRule, ToJSON) {
    mb::AccessRule rule("custom", "auth.id == '123'");
    
    auto json = rule.toJSON();
    
    EXPECT_EQ(json["mode"], "custom");
    EXPECT_EQ(json["expr"], "auth.id == '123'");
}

TEST(AccessRule, FromJSON) {
    nlohmann::json j = {
        {"mode", "public"},
        {"expr", ""}
    };
    
    auto rule = mb::AccessRule::fromJSON(j);
    
    EXPECT_EQ(rule.mode(), "public");
    EXPECT_EQ(rule.expr(), "");
    
    j = {
        {"mode", "custom"},
        {"expr", "auth.id != null"}
    };
    
    rule = mb::AccessRule::fromJSON(j);
    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "auth.id != null");
}

TEST(AccessRule, RoundTripJSON) {
    mb::AccessRule original("custom", "auth.entity == \"users\"");
    
    auto json = original.toJSON();
    auto restored = mb::AccessRule::fromJSON(json);
    
    EXPECT_EQ(restored.mode(), original.mode());
    EXPECT_EQ(restored.expr(), original.expr());
}

TEST(AccessRule, DifferentModes) {
    // Public mode
    mb::AccessRule publicRule("public", "");
    EXPECT_EQ(publicRule.mode(), "public");
    
    // Auth mode
    mb::AccessRule authRule("auth", "");
    EXPECT_EQ(authRule.mode(), "auth");
    
    // Custom mode
    mb::AccessRule customRule("custom", "auth.id == req.body.user_id");
    EXPECT_EQ(customRule.mode(), "custom");
    EXPECT_EQ(customRule.expr(), "auth.id == req.body.user_id");
    
    // Admin only (empty mode)
    mb::AccessRule adminRule("", "");
    EXPECT_EQ(adminRule.mode(), "");
    EXPECT_EQ(adminRule.expr(), "");
}

