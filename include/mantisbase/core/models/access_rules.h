/**
 * @file access_rules.h
 * @brief Access rule definition for entity permissions.
 *
 * Defines access control rules with mode and expression for
 * controlling read/write access to database entities.
 */

#ifndef MANTISBASE_ACCESS_RULES_H
#define MANTISBASE_ACCESS_RULES_H

#include <string>
#include <nlohmann/json.hpp>

namespace mb {
    /**
     * @brief Access control rule for entity permissions.
     *
     * Defines who can access entity operations using expression-based rules.
     * Rules consist of a mode (currently unused) and an expression string
     * that is evaluated against request context.
     *
     * @code
     * // Create rule: only authenticated users
     * AccessRule rule("", "auth.id != \"\"");
     *
     * // Create rule: admins only
     * AccessRule adminRule("", "auth.entity == \"mb_admins\"");
     * @endcode
     */
    class AccessRule {
    public:
        /**
         * @brief Construct access rule.
         * @param mode Rule mode (currently unused, reserved for future use)
         * @param expr Expression string to evaluate (e.g., "auth.id != \"\"")
         */
        explicit AccessRule(const std::string &mode = "", const std::string &expr = "");

        /**
         * @brief Convert rule to JSON representation.
         * @return JSON object with mode and expr fields
         */
        [[nodiscard]] nlohmann::json toJSON() const;
        
        /**
         * @brief Create rule from JSON object.
         * @param j JSON object with mode and expr fields
         * @return AccessRule instance
         */
        static AccessRule fromJSON(const nlohmann::json &j);

        /**
         * @brief Get rule mode.
         * @return Mode string
         */
        [[nodiscard]] std::string mode() const;
        
        /**
         * @brief Set rule mode.
         * @param _mode Mode string
         */
        void setMode(const std::string& _mode);

        /**
         * @brief Get expression string.
         * @return Expression string
         */
        [[nodiscard]] std::string expr() const;
        
        /**
         * @brief Set expression string.
         * @param _expr Expression string (e.g., "auth.id != \"\"")
         */
        void setExpr(const std::string& _expr);

    private:
        std::string m_mode, m_expr;
    };
} // mb

#endif //MANTISBASE_ACCESS_RULES_H