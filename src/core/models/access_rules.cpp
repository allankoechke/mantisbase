#include "../../../include/mantisbase/core/models/access_rules.h"
#include "mantisbase/core/exceptions.h"
#include "mantisbase/core/expr_evaluator.h"
#include "mantisbase/core/http.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <vector>

namespace mb {
    namespace {
        std::string trimCopy(const std::string &s) {
            size_t start = 0;
            while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
                ++start;
            }
            size_t end = s.size();
            while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
                --end;
            }
            return s.substr(start, end - start);
        }

        std::vector<std::string> splitCommaList(const std::string &value) {
            std::vector<std::string> tokens;
            std::stringstream ss(value);
            std::string part;
            while (std::getline(ss, part, ',')) {
                const auto trimmed = trimCopy(part);
                if (trimmed.empty()) {
                    throw MbException(400, "Invalid entity filter: empty token in comma-separated list!");
                }
                tokens.push_back(trimmed);
            }
            return tokens;
        }

        bool isValidEntityNameToken(const std::string &name) {
            static const std::regex pattern(R"(^[A-Za-z0-9_]{1,64}$)");
            return std::regex_match(name, pattern);
        }

        bool verificationVerified(const nlohmann::json &verification) {
            return verification.contains("verified") &&
                   verification["verified"].is_boolean() &&
                   verification["verified"].get<bool>();
        }

        bool hasAuthUser(const nlohmann::json &auth) {
            return auth.contains("user") && auth["user"].is_object() && !auth["user"].is_null();
        }

        std::string authEntityName(const nlohmann::json &auth) {
            if (auth.contains("entity") && auth["entity"].is_string()) {
                return auth["entity"].get<std::string>();
            }
            return {};
        }
    }

    bool isGuestAuth(const nlohmann::json &auth) {
        if (auth.empty()) {
            return true;
        }
        if (!auth.contains("type") || auth["type"].is_null()) {
            return true;
        }
        return auth["type"].get<std::string>() == "guest";
    }

    bool isAdminAuth(const nlohmann::json &auth) {
        return !auth.empty() && auth.contains("type") && auth["type"].is_string() &&
               auth["type"].get<std::string>() == "admin";
    }

    bool isUserAuth(const nlohmann::json &auth) {
        return !auth.empty() && auth.contains("type") && auth["type"].is_string() &&
               auth["type"].get<std::string>() == "user";
    }

    void MbAccessRule::validateEntityFilter(const std::string &entity) {
        if (entity.empty()) {
            return;
        }

        const auto tokens = splitCommaList(entity);
        for (const auto &token: tokens) {
            std::string name = token;
            if (name.starts_with("!")) {
                name = name.substr(1);
                if (name.empty()) {
                    throw MbException(400, "Invalid entity filter: negation requires an entity name!");
                }
            }
            if (!isValidEntityNameToken(name)) {
                throw MbException(400, "Invalid entity filter token `" + token +
                                               "`, expected entity name matching [A-Za-z0-9_]{1,64}!");
            }
        }
    }

    void MbAccessRule::validateEntityFilterForMode(const std::string &mode, const std::string &entity) {
        if (entity.empty()) {
            return;
        }
        if (mode != "auth") {
            throw MbException(400, "The `entity` field is only allowed when rule mode is `auth`!");
        }
        validateEntityFilter(entity);
    }

    MbAccessRule::MbAccessRule(const std::string &mode, const std::string &expr, const std::string &entity) {
        if (!(mode == "public" || mode == "auth" || mode == "custom" || mode.empty())) {
            throw MbException(400, "Expected rule to be `public`, `auth` or `custom` only!");
        }

        validateEntityFilterForMode(mode, entity);

        m_mode = mode;
        m_expr = mode == "custom" ? expr : "";
        m_entity = mode == "auth" ? entity : "";
    }

    nlohmann::json MbAccessRule::toJSON() const {
        nlohmann::json j = {
            {"mode", m_mode},
            {"expr", m_mode == "custom" ? m_expr : ""}
        };
        if (m_mode == "auth" && !m_entity.empty()) {
            j["entity"] = m_entity;
        }
        return j;
    }

    MbAccessRule MbAccessRule::fromJSON(const nlohmann::json &j) {
        const auto mode = j.is_null() ? "" : j.contains("mode") ? j["mode"].get<std::string>() : "";
        const auto expr = j.is_null() ? "" : j.contains("expr") ? j["expr"].get<std::string>() : "";

        std::string entity;
        if (!j.is_null() && j.contains("entity")) {
            if (!j["entity"].is_string()) {
                throw MbException(400, "Expected rule `entity` to be a comma-separated string!");
            }
            entity = j["entity"].get<std::string>();
        }

        return MbAccessRule{mode, expr, entity};
    }

    std::string MbAccessRule::mode() const { return m_mode; }

    void MbAccessRule::setMode(const std::string &_mode) {
        if (!(_mode == "public" || _mode == "auth" || _mode == "custom" || _mode.empty())) {
            throw MbException(400, "Expected rule to be empty, `public`, `auth` or `custom` only!");
        }

        if (_mode != "auth") {
            m_entity.clear();
        }

        m_mode = _mode;
    }

    std::string MbAccessRule::expr() const { return m_expr; }

    void MbAccessRule::setExpr(const std::string &_expr) {
        m_expr = _expr;
    }

    std::string MbAccessRule::entity() const { return m_entity; }

    void MbAccessRule::setEntity(const std::string &_entity) {
        validateEntityFilterForMode(m_mode, _entity);
        m_entity = _entity;
    }

    bool MbAccessRule::matchesAuthEntity(const std::string &userEntity) const {
        if (m_entity.empty()) {
            return true;
        }

        const auto tokens = splitCommaList(m_entity);
        bool has_inclusion = false;
        for (const auto &token: tokens) {
            if (!token.starts_with("!")) {
                has_inclusion = true;
                break;
            }
        }

        for (const auto &token: tokens) {
            if (token.starts_with("!")) {
                const auto excluded = token.substr(1);
                if (userEntity == excluded) {
                    return false;
                }
            } else if (userEntity == token) {
                if (has_inclusion) {
                    return true;
                }
            }
        }

        if (!has_inclusion) {
            return true;
        }

        return false;
    }

    nlohmann::json buildAccessExprVars(const MbRequest &req, const nlohmann::json &auth) {
        nlohmann::json body = nlohmann::json::object();
        try {
            if (req.method() == "POST" && !req.body().empty()) {
                const auto &[parsed, err] = req.bodyAsJson();
                if (err.empty()) {
                    body = parsed;
                }
            }
        } catch (...) {
        }

        return buildAccessExprVars(auth, req.remoteAddr(), static_cast<int>(req.remotePort()),
                                   req.localAddr(), static_cast<int>(req.localPort()), body);
    }

    nlohmann::json buildAccessExprVars(const nlohmann::json &auth, const std::string &remote_addr,
                                       const int remote_port, const std::string &local_addr, const int local_port,
                                       const nlohmann::json &body) {
        nlohmann::json vars = nlohmann::json::object();
        vars["auth"] = auth;

        nlohmann::json req_obj;
        req_obj["remoteAddr"] = remote_addr;
        req_obj["remotePort"] = remote_port;
        req_obj["localAddr"] = local_addr;
        req_obj["localPort"] = local_port;
        req_obj["body"] = body;
        vars["req"] = req_obj;
        return vars;
    }

    MbAccessEvalResult evaluateAccessRule(const MbAccessRule &rule, const MbAccessEvalContext &ctx) {
        if (isAdminAuth(ctx.auth)) {
            return MbAccessEvalResult::Allow;
        }

        if (rule.mode() == "public") {
            return MbAccessEvalResult::Allow;
        }

        if (rule.mode().empty()) {
            if (ctx.verification.empty() || !verificationVerified(ctx.verification)) {
                return MbAccessEvalResult::DenyUnauthenticated;
            }
            if (!hasAuthUser(ctx.auth)) {
                return MbAccessEvalResult::DenyUnauthenticated;
            }
            return MbAccessEvalResult::DenyForbidden;
        }

        if (rule.mode() == "auth") {
            if (ctx.verification.empty() || !verificationVerified(ctx.verification)) {
                return MbAccessEvalResult::DenyUnauthenticated;
            }
            if (!hasAuthUser(ctx.auth)) {
                return MbAccessEvalResult::DenyUnauthenticated;
            }
            if (!isUserAuth(ctx.auth) && !isAdminAuth(ctx.auth)) {
                return MbAccessEvalResult::DenyUnauthenticated;
            }
            if (!rule.matchesAuthEntity(authEntityName(ctx.auth))) {
                return MbAccessEvalResult::DenyForbidden;
            }
            return MbAccessEvalResult::Allow;
        }

        if (rule.mode() == "custom") {
            nlohmann::json vars;
            if (ctx.req != nullptr) {
                vars = buildAccessExprVars(*ctx.req, ctx.auth);
            } else {
                vars = buildAccessExprVars(ctx.auth, "", 0, "", 0);
            }

            if (Expr::eval(rule.expr(), vars)) {
                return MbAccessEvalResult::Allow;
            }
            return MbAccessEvalResult::DenyForbidden;
        }

        return MbAccessEvalResult::DenyUnknownRule;
    }

    std::pair<int, std::string> accessEvalHttpError(const MbAccessEvalResult result, const MbAccessRule &rule) {
        switch (result) {
            case MbAccessEvalResult::Allow:
                return {200, ""};
            case MbAccessEvalResult::DenyUnauthenticated:
                if (rule.mode().empty()) {
                    return {401, "Admin auth required to access this resource!"};
                }
                return {401, "Auth required to access this resource!"};
            case MbAccessEvalResult::DenyForbidden:
                if (rule.mode().empty()) {
                    return {403, "Admin auth required to access this resource."};
                }
                if (rule.mode() == "custom") {
                    return {403, "Access denied!"};
                }
                return {403, "Access denied!"};
            case MbAccessEvalResult::DenyUnknownRule:
                return {403, "Access denied, entity access rule unknown."};
        }
        return {403, "Access denied!"};
    }
} // mb
