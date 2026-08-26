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

    /** @brief Auth type helpers (work on request auth json; used by HTTP, SSE, WS). */
    [[nodiscard]] bool isGuestAuth(const nlohmann::json &auth);
    [[nodiscard]] bool isAdminAuth(const nlohmann::json &auth);
    [[nodiscard]] bool isUserAuth(const nlohmann::json &auth);

    enum class AccessEvalResult {
        Allow,
        DenyUnauthenticated,
        DenyForbidden,
        DenyUnknownRule
    };

    struct AccessEvalContext {
        const nlohmann::json &auth;
        const nlohmann::json &verification;
        MantisRequest *req = nullptr;
    };

    /**
     * @brief Access control rule for entity permissions.
     *
     * Modes: `public`, `auth` (optional comma-separated `entity` filter), `custom` (expr),
     * or empty string (admin only).
     */
    class AccessRule {
    public:
        explicit AccessRule(const std::string &mode = "", const std::string &expr = "",
                            const std::string &entity = "");

        [[nodiscard]] nlohmann::json toJSON() const;
        static AccessRule fromJSON(const nlohmann::json &j);

        [[nodiscard]] std::string mode() const;
        void setMode(const std::string &_mode);

        [[nodiscard]] std::string expr() const;
        void setExpr(const std::string &_expr);

        /** Comma-separated auth entity filter for `auth` mode (e.g. `users`, `users,editors`, `!guests`). */
        [[nodiscard]] std::string entity() const;
        void setEntity(const std::string &_entity);

        [[nodiscard]] bool matchesAuthEntity(const std::string &userEntity) const;

    private:
        static void validateEntityFilter(const std::string &entity);
        static void validateEntityFilterForMode(const std::string &mode, const std::string &entity);

        std::string m_mode, m_expr, m_entity;
    };

    [[nodiscard]] AccessEvalResult evaluateAccessRule(const AccessRule &rule, const AccessEvalContext &ctx);

    [[nodiscard]] nlohmann::json buildAccessExprVars(const MantisRequest &req, const nlohmann::json &auth);

    [[nodiscard]] nlohmann::json buildAccessExprVars(const nlohmann::json &auth, const std::string &remote_addr,
                                                     int remote_port, const std::string &local_addr, int local_port,
                                                     const nlohmann::json &body = nlohmann::json::object());

    /** @return `{status, error}` for HTTP error responses. */
    [[nodiscard]] std::pair<int, std::string> accessEvalHttpError(AccessEvalResult result,
                                                                  const AccessRule &rule = AccessRule{});
} // mb

#endif //MANTISBASE_ACCESS_RULES_H
