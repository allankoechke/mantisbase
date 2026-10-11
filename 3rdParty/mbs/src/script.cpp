#include "../include/mbs/script.hpp"

#include "evaluator.hpp"
#include "parser.hpp"

#include <exception>

namespace mbs {

TestResult test(std::string_view source, std::size_t max_source_length) {
  try {
    if (max_source_length > 0 && source.size() > max_source_length) {
      return TestResult{false, "source exceeds maximum length", std::nullopt};
    }
    const auto ast = parse(source);
    if (const auto err = semantic_check(*ast); err.has_value()) {
      return TestResult{false, *err, std::nullopt};
    }

    return TestResult{true, {}, std::nullopt};
  } catch (const ParseError& ex) {
    return TestResult{false, ex.what(), ex.loc};
  } catch (const std::exception& ex) {
    return TestResult{false, ex.what(), std::nullopt};
  }
}

EvalValueResult eval_val(const std::string_view source, const nlohmann::json& context,
                         std::size_t max_source_length) {
  try {
    if (max_source_length > 0 && source.size() > max_source_length) {
      return EvalErr{"source exceeds maximum length", std::nullopt};
    }
    const auto ast = parse(source);
    if (const auto err = semantic_check(*ast); err.has_value()) {
      return EvalErr{*err, std::nullopt};
    }

    return EvalOk{eval_expr(*ast, context)};
  } catch (const ParseError& ex) {
    return EvalErr{ex.what(), ex.loc};
  } catch (const std::exception& ex) {
    return EvalErr{ex.what(), std::nullopt};
  }
}

EvalBoolResult eval_true(const std::string_view source, const nlohmann::json& context,
                         std::size_t max_source_length) {
  const auto r = eval_val(source, context, max_source_length);

  // Check if it resulted in an error
  if (std::holds_alternative<EvalErr>(r)) {
    const auto&[error, loc] = std::get<EvalErr>(r);
    return EvalBoolResult{  false, false, error, loc};
  }

  // If all executed OK
  const auto&[value] = std::get<EvalOk>(r);
  return EvalBoolResult{true, json_truthy(value), {}, std::nullopt};
}

// --- Script class ---

CompiledExpr Script::compile(std::string_view source, std::size_t max_source_length) const {
  if (max_source_length > 0 && source.size() > max_source_length) {
    throw std::runtime_error("source exceeds maximum length");
  }
  auto ast = parse(source);
  const auto err = semantic_check(*ast, functions_);
  if (err.has_value()) {
    throw std::runtime_error(*err);
  }
  CompiledExpr compiled;
  compiled.ast_ = std::move(ast);
  compiled.source_ = std::string(source);
  return compiled;
}

EvalValueResult Script::eval_val(const CompiledExpr& compiled, const nlohmann::json& context) const {
  try {
    if (!compiled.ast_) {
      return EvalErr{"expression not compiled", std::nullopt};
    }
    return EvalOk{eval_expr(*compiled.ast_, context, functions_)};
  } catch (const ParseError& ex) {
    return EvalErr{ex.what(), ex.loc};
  } catch (const std::exception& ex) {
    return EvalErr{ex.what(), std::nullopt};
  }
}

EvalBoolResult Script::eval_true(const CompiledExpr& compiled, const nlohmann::json& context) const {
  const auto r = eval_val(compiled, context);
  if (std::holds_alternative<EvalErr>(r)) {
    const auto&[error, loc] = std::get<EvalErr>(r);
    return EvalBoolResult{false, false, error, loc};
  }
  const auto&[value] = std::get<EvalOk>(r);
  return EvalBoolResult{true, json_truthy(value), {}, std::nullopt};
}

EvalValueResult Script::eval_val(std::string_view source, const nlohmann::json& context,
                                 std::size_t max_source_length) const {
  try {
    if (max_source_length > 0 && source.size() > max_source_length) {
      return EvalErr{"source exceeds maximum length", std::nullopt};
    }
    const auto ast = parse(source);
    if (const auto err = semantic_check(*ast, functions_); err.has_value()) {
      return EvalErr{*err, std::nullopt};
    }
    return EvalOk{eval_expr(*ast, context, functions_)};
  } catch (const ParseError& ex) {
    return EvalErr{ex.what(), ex.loc};
  } catch (const std::exception& ex) {
    return EvalErr{ex.what(), std::nullopt};
  }
}

EvalBoolResult Script::eval_true(std::string_view source, const nlohmann::json& context,
                                 std::size_t max_source_length) const {
  const auto r = eval_val(source, context, max_source_length);
  if (std::holds_alternative<EvalErr>(r)) {
    const auto&[error, loc] = std::get<EvalErr>(r);
    return EvalBoolResult{false, false, error, loc};
  }
  const auto&[value] = std::get<EvalOk>(r);
  return EvalBoolResult{true, json_truthy(value), {}, std::nullopt};
}

void Script::register_function(std::string name, FunctionDef fn) {
  functions_[std::move(name)] = std::move(fn);
}

}  // namespace mbs
