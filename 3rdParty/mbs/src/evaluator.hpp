#pragma once

#include "ast.hpp"

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mbs {

inline constexpr std::size_t MAX_STRING_SIZE = 1 * 1024 * 1024;
inline constexpr int MAX_EVAL_DEPTH = 512;

struct EvalError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

using FunctionDef = std::function<nlohmann::json(
    const nlohmann::json& target,
    const std::vector<nlohmann::json>& args)>;

using FunctionRegistry = std::unordered_map<std::string, FunctionDef>;

std::optional<std::string> semantic_check(const ast::Expr& expr);
std::optional<std::string> semantic_check(const ast::Expr& expr, const FunctionRegistry& funcs);

nlohmann::json eval_expr(const ast::Expr& expr, const nlohmann::json& context);
nlohmann::json eval_expr(const ast::Expr& expr, const nlohmann::json& context,
                         const FunctionRegistry& funcs);

bool json_truthy(const nlohmann::json& j);

}  // namespace mbs
