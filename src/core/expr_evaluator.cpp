#include "../../include/mantisbase/core/expr_evaluator.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <mbs/script.hpp>

namespace mb {
    bool Expr::eval(const std::string &expr, const nlohmann::json &vars) {
        if (trim(expr).empty()) return false;
        const auto res = mbs::eval_true(expr, vars);
        return res.value && res.ok;
    }
} // mantis
