//
// Created by codeart on 13/11/2025.
//

#ifndef MANTISBASE_ENTITY_SCHEMA_FIELD_H
#define MANTISBASE_ENTITY_SCHEMA_FIELD_H

#include <optional>
#include <string>

#include "nlohmann/json.hpp"
#include "soci/soci-backend.h"

namespace mantis {
    class EntitySchemaField {
    public:
        static const std::vector<std::string>& defaultBaseFields();
        static const std::vector<std::string>& defaultAuthFields();
        static const std::vector<std::string>& defaultEntityFieldTypes();

        // Convenience constructor
        EntitySchemaField(std::string field_name, std::string field_type);

        explicit EntitySchemaField(const nlohmann::json &field_schema);

        void updateWith(const nlohmann::json &field_schema);

        // ----------------- SCHEMA FIELD METHODS ---------------------- //
        [[nodiscard]] std::string id() const;

        EntitySchemaField &setId(const std::string &id);

        [[nodiscard]] std::string name() const;

        EntitySchemaField &setName(const std::string &name);

        [[nodiscard]] std::string type() const;

        EntitySchemaField &setType(const std::string &type);

        [[nodiscard]] bool required() const;

        EntitySchemaField &setRequired(bool required);

        [[nodiscard]] bool isPrimaryKey() const;

        EntitySchemaField &setIsPrimaryKey(bool pk);

        [[nodiscard]] bool isSystem() const;

        EntitySchemaField &setIsSystem(bool system);

        [[nodiscard]] bool isUnique() const;

        EntitySchemaField &setIsUnique(bool unique);

        [[nodiscard]] nlohmann::json constraints() const;

        [[nodiscard]] nlohmann::json constraints(const std::string& key) const;

        EntitySchemaField &setConstraints(const nlohmann::json &opts);

        // ----------------- SCHEMA FIELD OPS ---------------------- //
        [[nodiscard]] nlohmann::json toJson() const;

        [[nodiscard]] soci::db_type toSociType() const;

        [[nodiscard]] static soci::db_type toSociType(const std::string &type);

        [[nodiscard]] std::optional<std::string> validate() const;

        static bool isValidFieldType(const std::string &type);

        static std::string genFieldId(const std::string& id);

    private:
        std::string m_id, m_name, m_type;
        bool m_required = false, m_primaryKey = false, m_isSystem = false, m_isUnique = false;
        nlohmann::json m_constraints = {
            {"min_value", nullptr},
            {"max_value", nullptr},
            {"validator", nullptr},
            {"default_value", nullptr}
        };
    };
} // mantis

#endif //MANTISBASE_ENTITY_SCHEMA_FIELD_H
