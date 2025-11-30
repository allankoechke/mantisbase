#include <gtest/gtest.h>
#include "mantisbase/core/expr_evaluator.h"

TEST(ExprEval, CustomExprEval) {
    EXPECT_FALSE(mantis::Expr::eval(""));

    mantis::TokenMap v;
    v["auth"] = nullptr;
    EXPECT_FALSE(mantis::Expr::eval("auth.id == '123'", v));
    EXPECT_FALSE(mantis::Expr::eval("auth != null && auth.id != null", v));
    EXPECT_FALSE(mantis::Expr::eval("auth != null && auth.id == '123'", v));

    v["auth"] = {{"id", "123"}};
    EXPECT_TRUE(mantis::Expr::eval("auth.id == '123'", v));
    EXPECT_TRUE(mantis::Expr::eval("auth.id != null", v));
}