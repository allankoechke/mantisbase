#include "../../include/mantisbase/core/expr_evaluator.h"
#include <nlohmann/json.hpp>
#include <mbs/script.hpp>

namespace mb {
    bool Expr::eval(const std::string &expr, const nlohmann::json &vars) {
        return trim(expr).empty() ? false : mbs::eval_true(expr, vars).value;
    }
} // mantis
