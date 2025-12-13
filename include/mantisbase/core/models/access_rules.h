#ifndef MANTISBASE_ACCESS_RULES_H
#define MANTISBASE_ACCESS_RULES_H

#include <string>
#include <nlohmann/json.hpp>

namespace mb {
    class AccessRule {
    public:
        explicit AccessRule(const std::string &mode = "", const std::string &expr = "");

        [[nodiscard]] nlohmann::json toJSON() const;
        static AccessRule fromJSON(const nlohmann::json &j);

        [[nodiscard]] std::string mode() const;
        void setMode(const std::string& _mode);

        [[nodiscard]] std::string expr() const;
        void setExpr(const std::string& _expr);

    private:
        std::string m_mode, m_expr;
    };
} // mantis

#endif //MANTISBASE_ACCESS_RULES_H