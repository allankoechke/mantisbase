#ifndef MANTISBASE_ACCESS_RULES_H
#define MANTISBASE_ACCESS_RULES_H

#include <string>
#include <nlohmann/json.hpp>

namespace mantis {
    class AccessRule {
    public:
        AccessRule() = default;
        explicit AccessRule(const std::string &mode, const std::string &expr = "");

        nlohmann::json toJSON() const;
        static AccessRule fromJSON(const nlohmann::json &j);

        std::string mode() const;
        void setMode(const std::string& _mode);

        std::string expr() const;
        void setExpr(const std::string& _expr);


    private:
        std::string m_mode = "auth", m_expr;
    };
} // mantis

#endif //MANTISBASE_ACCESS_RULES_H