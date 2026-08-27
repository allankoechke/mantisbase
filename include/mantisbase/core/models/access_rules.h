/**
 * @file access_rules.h
 * @brief Access rule definition and shared evaluation for entity permissions.
 */

#ifndef MANTISBASE_ACCESS_RULES_H
#define MANTISBASE_ACCESS_RULES_H

#include <string>
#include <utility>
#include <nlohmann/json.hpp>

namespace mb {
    class MantisRequest;

    /** @return `true` when @p auth represents an unauthenticated guest. */
    [[nodiscard]] bool isGuestAuth(const nlohmann::json &auth);
    /** @return `true` when @p auth represents an `mb_admins` session. */
    [[nodiscard]] bool isAdminAuth(const nlohmann::json &auth);
    /** @return `true` when @p auth represents a regular entity user session. */
    [[nodiscard]] bool isUserAuth(const nlohmann::json &auth);

    /** Outcome of @ref evaluateAccessRule for HTTP, SSE, and WebSocket checks. */
    enum class AccessEvalResult {
        Allow,                  ///< Rule permits access
        DenyUnauthenticated,    ///< Credentials required but missing/invalid
        DenyForbidden,          ///< Authenticated but rule rejected the caller
        DenyUnknownRule         ///< Unrecognized rule mode or malformed config
    };

    /** Inputs shared by entity list/get/create/update/delete access checks. */
    struct AccessEvalContext {
        const nlohmann::json &auth;          ///< Resolved auth block from middleware
        const nlohmann::json &verification;  ///< JWT/API-key verification metadata
        MantisRequest *req = nullptr;        ///< Optional HTTP request (custom rules)
    };

    /**
     * @brief Access control rule for entity permissions.
     *
     * Modes: `public`, `auth` (optional comma-separated `entity` filter), `custom` (expr),
     * or empty string (admin only).
     */
    class AccessRule {
    public:
        /** Construct a rule from mode, optional custom expression, and optional entity filter. */
        explicit AccessRule(const std::string &mode = "", const std::string &expr = "",
                            const std::string &entity = "");

        /** Serialize rule to JSON (`mode`, `expr`, `entity`). */
        [[nodiscard]] nlohmann::json toJSON() const;
        /** Parse rule from stored schema JSON. */
        static AccessRule fromJSON(const nlohmann::json &j);

        /** @return Rule mode: `public`, `auth`, `custom`, or empty (admin-only). */
        [[nodiscard]] std::string mode() const;
        void setMode(const std::string &_mode);

        /** @return Custom expression when mode is `custom`. */
        [[nodiscard]] std::string expr() const;
        void setExpr(const std::string &_expr);

        /** Comma-separated auth entity filter for `auth` mode (e.g. `users`, `users,editors`, `!guests`). */
        [[nodiscard]] std::string entity() const;
        void setEntity(const std::string &_entity);

        /** Check whether @p userEntity satisfies the `auth` mode entity filter. */
        [[nodiscard]] bool matchesAuthEntity(const std::string &userEntity) const;

    private:
        static void validateEntityFilter(const std::string &entity);
        static void validateEntityFilterForMode(const std::string &mode, const std::string &entity);

        std::string m_mode, m_expr, m_entity;
    };

    /** Evaluate @p rule against @p ctx (auth, verification, optional request). */
    [[nodiscard]] AccessEvalResult evaluateAccessRule(const AccessRule &rule, const AccessEvalContext &ctx);

    /** Build expression variables from an HTTP request and auth block (custom rules). */
    [[nodiscard]] nlohmann::json buildAccessExprVars(const MantisRequest &req, const nlohmann::json &auth);

    /** Build expression variables for non-HTTP contexts (SSE/WS, tests). */
    [[nodiscard]] nlohmann::json buildAccessExprVars(const nlohmann::json &auth, const std::string &remote_addr,
                                                     int remote_port, const std::string &local_addr, int local_port,
                                                     const nlohmann::json &body = nlohmann::json::object());

    /** @return `{status, error}` for HTTP error responses. */
    [[nodiscard]] std::pair<int, std::string> accessEvalHttpError(AccessEvalResult result,
                                                                  const AccessRule &rule = AccessRule{});
} // mb

#endif //MANTISBASE_ACCESS_RULES_H
