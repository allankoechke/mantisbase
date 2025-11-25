#ifndef MANTISBASE_ENTITY_H
#define MANTISBASE_ENTITY_H

#include <string>
#include "mantisbase/mantis.h"
#include "mantisbase/core/exceptions.h"
#include "mantisbase/utils/soci_wrappers.h"
#include "../types.h"
#include "access_rules.h"

namespace mantis {
    using Record = nlohmann::json;
    using Records = std::vector<Record>;

    class Entity {
    public:
        /**
         * @brief `Entity`
         * @param schema Table schema
         */
        explicit Entity(const nlohmann::json &schema);
        explicit Entity(const std::string &name, const std::string& type);

        // --------------- DB TABLE OPS ------------------ //
        [[nodiscard]] std::string id() const;

        [[nodiscard]] std::string name() const;

        [[nodiscard]] std::string type() const;

        [[nodiscard]] bool isSystem() const;

        [[nodiscard]] bool hasApi() const;

        [[nodiscard]] std::string viewQuery() const;

        [[nodiscard]] const std::vector<json> &fields() const;
        [[nodiscard]] std::optional<json> field(const std::string& field_name) const;
        [[nodiscard]] std::optional<json> hasField(const std::string& field_name) const;

        [[nodiscard]] const json& rules() const;

        [[nodiscard]] AccessRule listRule() const;

        [[nodiscard]] AccessRule getRule() const;

        [[nodiscard]] AccessRule addRule() const;

        [[nodiscard]] AccessRule updateRule() const;

        [[nodiscard]] AccessRule deleteRule() const;

        // --------------- DB CRUD OPS ------------------ //
        [[nodiscard]] Record create(const json &Record, const json &opts = json::object()) const;

        [[nodiscard]] Records list(const json &opts = json::object()) const;

        [[nodiscard]] std::optional<Record> read(const std::string &id, const json &opts = json::object()) const;

        [[nodiscard]] Record update(const std::string &id, const json &data, const json &opts = json::object()) const;

        void remove(const std::string &id) const;

        // --------------- SCHEMA OPS ------------------ //
        [[nodiscard]] const json &schema() const;

        // --------------- UTILITY OPS ------------------ //
        [[nodiscard]] bool recordExists(const std::string &id) const;

        [[nodiscard]] std::optional<json> findField(const std::string &field_name) const;

        [[nodiscard]] std::optional<json> queryFromCols(const std::string &value,
                                                        const std::vector<std::string> &columns) const;

        // --------------- SCHEMA ROUTING ------------------ //
        [[nodiscard]] HandlerFn getOneRouteHandler() const;

        [[nodiscard]] HandlerFn getManyRouteHandler() const;

        [[nodiscard]] HandlerFn postRouteHandler() const;

        [[nodiscard]] HandlerFn patchRouteHandler() const;

        [[nodiscard]] HandlerFn deleteRouteHandler() const;

        [[nodiscard]] HandlerFn authRouteHandler() const;

        void createEntityRoutes() const;

    private:
        nlohmann::json m_schema;
    };
} // mantis

#endif //MANTISBASE_ENTITY_H
