#include "../../../include/mantisbase/core/models/access_rules.h"
#include "mantisbase/core/exceptions.h"

namespace mb {
    AccessRule::AccessRule(const std::string &mode, const std::string &expr) {
        if (!(mode == "public" || mode == "auth" || mode == "custom" || mode.empty())) {
            throw MantisException(400, "Expected rule to be `public`, `auth` or `custom` only!");
        }

        m_mode = mode;
        m_expr = mode == "custom" ? expr : ""; // Reset `expr` for non-custom mode types
    }

    nlohmann::json AccessRule::toJSON() const {
        return {
            {"mode", m_mode},
            {"expr", m_mode == "custom" ? m_expr : ""} // Ensure its empty for non-custom types
        };
    }

    AccessRule AccessRule::fromJSON(const nlohmann::json &j) {
        const auto mode = j.is_null() ? "" : j.contains("mode") ? j["mode"].get<std::string>() : "";
        const auto expr = j.is_null() ? "" : j.contains("expr") ? j["expr"].get<std::string>() : "";
        return AccessRule{mode, expr};
    }

    std::string AccessRule::mode() const { return m_mode; }

    void AccessRule::setMode(const std::string &_mode) {
        if (!(_mode == "public" || _mode == "auth" || _mode == "custom" || _mode.empty())) {
            throw MantisException(400, "Expected rule to be empty, `public`, `auth` or `custom` only!");
        }

        m_mode = _mode;
    }

    std::string AccessRule::expr() const { return m_expr; }

    void AccessRule::setExpr(const std::string &_expr) {
        m_expr = _expr;
    }
} // mantis
