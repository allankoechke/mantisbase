/**
 * @file expr_evaluator.h
 * @brief Expression Evaluator Unit for database access rules.
 */

#ifndef EXPR_EVALUATOR_H
#define EXPR_EVALUATOR_H

#include <string>
#include <shunting-yard.h>
#include <containers.h>
#include <nlohmann/json.hpp>

#include "../utils/utils.h"

namespace mantis
{
    using cparse::TokenMap;
    using cparse::calculator;
    using cparse::packToken;
    using json = nlohmann::json;

    /**
     * @brief Struct instance for handling evaluation of database access rules.
     */
    struct ExprMgr
    {
        ExprMgr() = default;

        /**
         * @brief Evaluates a given expression in a context of the given TokenMap variables.
         *
         * @param expr Access rule expression
         * @param vars Parameter tokens
         * @return True or False
         */
        auto evaluate(const std::string& expr, const TokenMap& vars) -> bool;

        /**
         * @brief Evaluates a given expression in a context of the given JSON object variables.
         *
         * @param expr Access rule expression.
         * @param vars Parameter variables as JSON.
         * @return True or False
         */
        auto evaluate(const std::string& expr, const json& vars) -> bool;

        /**
         * @brief Convert a given JSON Object to the equivalent TokenMap so that we can pass it to the evaluator.
         *
         * @param j JSON Object
         * @return TokenMap equivalent of the JSON Object
         */
        auto jsonToTokenMap(const json& j) -> TokenMap;

        const std::string __class_name__ = "mantis::ExprEvaluator";
    };
} // mantis

#endif //EXPR_EVALUATOR_H
