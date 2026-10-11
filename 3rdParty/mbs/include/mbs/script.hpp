#pragma once

#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace mbs {

    namespace ast { struct Expr; }

    struct SourceLocation {
        std::size_t line{1};
        std::size_t column{1};
    };

    struct TestResult {
        bool ok{false};
        std::string error;
        std::optional<SourceLocation> loc;
    };

    /// Lex + parse + static checks (unknown methods / bad arity).
    TestResult test(std::string_view source, std::size_t max_source_length = 0);

    struct EvalOk {
        nlohmann::json value;
    };

    struct EvalErr {
        std::string error;
        std::optional<SourceLocation> loc;
    };

    using EvalValueResult = std::variant<EvalOk, EvalErr>;

    EvalValueResult eval_val(std::string_view source, const nlohmann::json &context,
                             std::size_t max_source_length = 0);

    struct EvalBoolResult {
        bool ok{false};
        bool value{false};
        std::string error;
        std::optional<SourceLocation> loc;
    };

    EvalBoolResult eval_true(std::string_view source, const nlohmann::json &context,
                             std::size_t max_source_length = 0);

    using FunctionDef = std::function<nlohmann::json(
        const nlohmann::json& target,
        const std::vector<nlohmann::json>& args)>;

    using FunctionRegistry = std::unordered_map<std::string, FunctionDef>;

    class CompiledExpr {
    public:
        CompiledExpr() = default;
        explicit operator bool() const { return static_cast<bool>(ast_); }
    private:
        friend class Script;
        std::shared_ptr<const ast::Expr> ast_;
        std::string source_;
    };

    class Script {
    public:
        Script() = default;

        CompiledExpr compile(std::string_view source, std::size_t max_source_length = 0) const;

        EvalValueResult eval_val(const CompiledExpr& compiled, const nlohmann::json& context) const;
        EvalBoolResult  eval_true(const CompiledExpr& compiled, const nlohmann::json& context) const;

        EvalValueResult eval_val(std::string_view source, const nlohmann::json& context,
                                 std::size_t max_source_length = 0) const;
        EvalBoolResult  eval_true(std::string_view source, const nlohmann::json& context,
                                  std::size_t max_source_length = 0) const;

        void register_function(std::string name, FunctionDef fn);

    private:
        FunctionRegistry functions_;
    };
} // namespace mbs
