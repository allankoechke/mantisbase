#include <gtest/gtest.h>
#include "mantisbase/core/expr_evaluator.h"

TEST(ExprEval, EmptyExpression) {
    EXPECT_FALSE(mb::Expr::eval(""));
    
    mb::TokenMap v;
    EXPECT_FALSE(mb::Expr::eval("", v));
}

TEST(ExprEval, NullAuthContext) {
    mb::TokenMap v;
    v["auth"] = nullptr;
    
    EXPECT_FALSE(mb::Expr::eval("auth.id == '123'", v));
    EXPECT_FALSE(mb::Expr::eval("auth != null && auth.id != null", v));
    EXPECT_FALSE(mb::Expr::eval("auth != null && auth.id == '123'", v));
    EXPECT_FALSE(mb::Expr::eval("auth.id != null", v));
}

TEST(ExprEval, ValidAuthContext) {
    mb::TokenMap v;
    v["auth"] = {{"id", "123"}, {"entity", "users"}};
    
    EXPECT_TRUE(mb::Expr::eval("auth.id == '123'", v));
    EXPECT_TRUE(mb::Expr::eval("auth.id != null", v));
    EXPECT_TRUE(mb::Expr::eval("auth.entity == 'users'", v));
    EXPECT_FALSE(mb::Expr::eval("auth.id == '456'", v));
    EXPECT_FALSE(mb::Expr::eval("auth.entity == 'admins'", v));
}

TEST(ExprEval, ComplexExpressions) {
    mb::TokenMap v;
    v["auth"] = {{"id", "123"}, {"entity", "users"}, {"type", "user"}};
    v["req"] = {{"body", {{"user_id", "123"}}}};
    
    EXPECT_TRUE(mb::Expr::eval("auth.id == '123' && auth.entity == 'users'", v));
    EXPECT_TRUE(mb::Expr::eval("auth.id != null && auth.entity != null", v));
    EXPECT_TRUE(mb::Expr::eval("auth.id == req.body.user_id", v));
    EXPECT_FALSE(mb::Expr::eval("auth.id == '456' || auth.entity == 'admins'", v));
}

TEST(ExprEval, RequestContext) {
    mb::TokenMap v;
    v["auth"] = {{"id", "123"}};
    v["req"] = {{"remoteAddr", "127.0.0.1"}, {"body", {{"title", "Test"}}}};
    
    EXPECT_TRUE(mb::Expr::eval("req.remoteAddr == '127.0.0.1'", v));
    EXPECT_TRUE(mb::Expr::eval("req.body.title == 'Test'", v));
    EXPECT_FALSE(mb::Expr::eval("req.remoteAddr == '192.168.1.1'", v));
}

TEST(ExprEval, InvalidExpressions) {
    mb::TokenMap v;
    v["auth"] = {{"id", "123"}};
    
    // Invalid syntax should return false
    EXPECT_FALSE(mb::Expr::eval("auth.id ===", v));
    EXPECT_FALSE(mb::Expr::eval("auth.nonexistent.field", v));
    EXPECT_FALSE(mb::Expr::eval("invalid javascript syntax {", v));
}