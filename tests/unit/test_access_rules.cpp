#include <gtest/gtest.h>
#include "mantisbase/core/models/access_rules.h"
#include "mantisbase/core/exceptions.h"
#include <nlohmann/json.hpp>

namespace {
    nlohmann::json verifiedUserAuth(const std::string &entity = "users") {
        return {
            {"type", "user"},
            {"entity", entity},
            {"id", "user-1"},
            {"user", {{"id", "user-1"}, {"entity", entity}}}
        };
    }

    nlohmann::json verifiedAdminAuth() {
        return {
            {"type", "admin"},
            {"entity", "mb_admins"},
            {"id", "admin-1"},
            {"user", {{"id", "admin-1"}, {"entity", "mb_admins"}}}
        };
    }

    nlohmann::json verifiedJson() {
        return {{"verified", true}, {"error", ""}};
    }
}

TEST(AuthTypeHelpers, GuestAdminUser) {
    EXPECT_TRUE(mb::isGuestAuth(nlohmann::json::object()));
    EXPECT_TRUE(mb::isGuestAuth({{"type", "guest"}}));
    EXPECT_FALSE(mb::isAdminAuth({{"type", "guest"}}));
    EXPECT_FALSE(mb::isUserAuth({{"type", "guest"}}));

    EXPECT_TRUE(mb::isAdminAuth({{"type", "admin"}}));
    EXPECT_TRUE(mb::isUserAuth({{"type", "user"}}));
}

TEST(AccessRule, DefaultConstructor) {
    const mb::AccessRule rule;

    EXPECT_EQ(rule.mode(), "");
    EXPECT_EQ(rule.expr(), "");
    EXPECT_EQ(rule.entity(), "");
}

TEST(AccessRule, AuthEntityFilter) {
    const mb::AccessRule rule("auth", "", "users,editors");
    EXPECT_EQ(rule.mode(), "auth");
    EXPECT_EQ(rule.entity(), "users,editors");
    EXPECT_TRUE(rule.matchesAuthEntity("users"));
    EXPECT_TRUE(rule.matchesAuthEntity("editors"));
    EXPECT_FALSE(rule.matchesAuthEntity("guests"));

    const mb::AccessRule negRule("auth", "", "!guests");
    EXPECT_TRUE(negRule.matchesAuthEntity("users"));
    EXPECT_FALSE(negRule.matchesAuthEntity("guests"));
}

TEST(AccessRule, AuthEntityValidation) {
    EXPECT_THROW(mb::AccessRule("auth", "", "bad name"), mb::MantisException);
    EXPECT_THROW(mb::AccessRule("public", "", "users"), mb::MantisException);

    mb::AccessRule rule("public");
    EXPECT_THROW(rule.setEntity("users"), mb::MantisException);
    rule.setMode("auth");
    EXPECT_NO_THROW(rule.setEntity("users"));
}

TEST(AccessRule, EntityJsonRoundTrip) {
    nlohmann::json j = {
        {"mode", "auth"},
        {"entity", "users,editors"}
    };

    const auto rule = mb::AccessRule::fromJSON(j);
    EXPECT_EQ(rule.entity(), "users,editors");

    const auto out = rule.toJSON();
    EXPECT_EQ(out["mode"], "auth");
    EXPECT_EQ(out["entity"], "users,editors");
    EXPECT_TRUE(out.contains("expr"));

    const mb::AccessRule wildcard("auth");
    EXPECT_FALSE(wildcard.toJSON().contains("entity"));
}

TEST(AccessRule, FromJSONRejectsNonStringEntity) {
    nlohmann::json j = {
        {"mode", "auth"},
        {"entity", nlohmann::json::array({"users"})}
    };
    EXPECT_THROW(mb::AccessRule::fromJSON(j), mb::MantisException);
}

TEST(AccessRule, ConstructorWithParams) {
    const mb::AccessRule rule("custom", "@auth.id != \"\"");

    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "@auth.id != \"\"");
}

TEST(AccessRule, SetModeAndExpr) {
    mb::AccessRule rule;

    rule.setMode("public");
    rule.setExpr("");

    EXPECT_EQ(rule.mode(), "public");
    EXPECT_EQ(rule.expr(), "");

    rule.setMode("auth");
    rule.setEntity("users");
    EXPECT_EQ(rule.entity(), "users");

    rule.setMode("custom");
    EXPECT_EQ(rule.entity(), "");
    rule.setExpr("@auth.entity == \"mb_admins\"");
    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "@auth.entity == \"mb_admins\"");
}

TEST(AccessRule, ToJSON) {
    const mb::AccessRule rule("custom", "@auth.id == '123'");
    auto json = rule.toJSON();

    EXPECT_EQ(json["mode"], "custom");
    EXPECT_EQ(json["expr"], "@auth.id == '123'");
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
        {"expr", "@auth.id != null"}
    };

    rule = mb::AccessRule::fromJSON(j);
    EXPECT_EQ(rule.mode(), "custom");
    EXPECT_EQ(rule.expr(), "@auth.id != null");
}

TEST(AccessRule, RoundTripJSON) {
    mb::AccessRule original("custom", "@auth.entity == \"users\"");

    const auto json = original.toJSON();
    const auto restored = mb::AccessRule::fromJSON(json);

    EXPECT_EQ(restored.mode(), original.mode());
    EXPECT_EQ(restored.expr(), original.expr());
}

TEST(AccessRule, DifferentModes) {
    mb::AccessRule publicRule("public", "");
    EXPECT_EQ(publicRule.mode(), "public");

    mb::AccessRule authRule("auth", "");
    EXPECT_EQ(authRule.mode(), "auth");

    mb::AccessRule customRule("custom", "@auth.id == @req.body.user_id");
    EXPECT_EQ(customRule.mode(), "custom");
    EXPECT_EQ(customRule.expr(), "@auth.id == @req.body.user_id");

    mb::AccessRule adminRule("", "");
    EXPECT_EQ(adminRule.mode(), "");
    EXPECT_EQ(adminRule.expr(), "");
}

TEST(EvaluateAccessRule, AdminShortCircuits) {
    const mb::AccessRule customRule("custom", "false");
    const mb::AccessRule authRule("auth", "", "users");

    mb::AccessEvalContext ctx{verifiedAdminAuth(), verifiedJson(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(customRule, ctx), mb::AccessEvalResult::Allow);
    EXPECT_EQ(mb::evaluateAccessRule(authRule, ctx), mb::AccessEvalResult::Allow);
}

TEST(EvaluateAccessRule, PublicAllowsGuest) {
    const mb::AccessRule rule("public");
    mb::AccessEvalContext ctx{nlohmann::json{{"type", "guest"}}, nlohmann::json::object(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(rule, ctx), mb::AccessEvalResult::Allow);
}

TEST(EvaluateAccessRule, AuthEntityFilter) {
    const mb::AccessRule rule("auth", "", "users");
    mb::AccessEvalContext allowed{verifiedUserAuth("users"), verifiedJson(), nullptr};
    mb::AccessEvalContext denied{verifiedUserAuth("editors"), verifiedJson(), nullptr};

    EXPECT_EQ(mb::evaluateAccessRule(rule, allowed), mb::AccessEvalResult::Allow);
    EXPECT_EQ(mb::evaluateAccessRule(rule, denied), mb::AccessEvalResult::DenyForbidden);
}

TEST(EvaluateAccessRule, AdminOnlyDeniesUser) {
    const mb::AccessRule rule("");
    mb::AccessEvalContext ctx{verifiedUserAuth("users"), verifiedJson(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(rule, ctx), mb::AccessEvalResult::DenyForbidden);
}

TEST(EvaluateAccessRule, AuthRequiresVerification) {
    const mb::AccessRule rule("auth");
    mb::AccessEvalContext ctx{verifiedUserAuth("users"), nlohmann::json::object(), nullptr};
    EXPECT_EQ(mb::evaluateAccessRule(rule, ctx), mb::AccessEvalResult::DenyUnauthenticated);
}
