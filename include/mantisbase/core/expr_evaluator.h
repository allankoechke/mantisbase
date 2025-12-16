/**
 * @file expr_evaluator.h
 * @brief Expression Evaluator Unit for database access rules.
 */

#ifndef EXPR_EVALUATOR_H
#define EXPR_EVALUATOR_H

#include <string>
#include <nlohmann/json.hpp>
#include <dukglue/dukglue.h>

#include "../utils/utils.h"

namespace mb
{
    using TokenMap = std::unordered_map<std::string, nlohmann::json>;

    class DukCtx {
    public:
        DukCtx();

        ~DukCtx();

        [[nodiscard]] duk_context* get() const;

    private:
        duk_context* m_ctx;
    };

    /**
     * @brief Struct instance for handling evaluation of database access rules.
     */
    struct Expr
    {
        /**
         * @brief Evaluates a given expression in a context of the given TokenMap variables.
         *
         * @param expr Access rule expression
         * @param vars Parameter tokens
         * @return True or False
         */
        static bool eval(const std::string& expr, const std::unordered_map<std::string, nlohmann::json>& vars = {});
    };
} // mb

#endif //EXPR_EVALUATOR_H
