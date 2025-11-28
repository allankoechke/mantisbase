//
// Created by codeart on 12/11/2025.
//

#include "../../../include/mantisbase/core/models/entity_schema.h"
#include "../../../include/mantisbase/core/models/entity_schema_field.h"

namespace mantis {
    const std::vector<EntitySchemaField> &EntitySchema::defaultBaseFieldsSchema() {
        static const std::vector _base_field_schema = {
            EntitySchemaField{
                {
                    {"name", "id"},
                    {"type", "string"},
                    {"required", true},
                    {"primary_key", true},
                    {"system", true},
                    {"unique", false},
                    {
                        "constraints", {
                            {"min-value", 6},
                            {"max_value", nullptr},
                            {"validator", "@password"},
                            {"default_value", nullptr},
                        }
                    },
                }
            },
            EntitySchemaField{
                {
                    {"name", "created"},
                    {"type", "date"},
                    {"required", true},
                    {"primary_key", false},
                    {"system", true},
                    {"unique", false},
                    {
                        "constraints", {
                            {"min-value", nullptr},
                            {"max_value", nullptr},
                            {"validator", nullptr},
                            {"default_value", nullptr},
                        }
                    },
                }
            },
            EntitySchemaField{
                {
                    {"name", "updated"},
                    {"type", "date"},
                    {"required", true},
                    {"primary_key", false},
                    {"system", true},
                    {"unique", false},
                    {
                        "constraints", {
                            {"min-value", nullptr},
                            {"max_value", nullptr},
                            {"validator", nullptr},
                            {"default_value", nullptr},
                        }
                    },
                }
            }
        };
        return _base_field_schema;
    }

    const std::vector<EntitySchemaField> &EntitySchema::defaultAuthFieldsSchema() {
        static const std::vector _auth_field_schema = {
            EntitySchemaField{
                    {
                        {"name", "id"},
                        {"type", "string"},
                        {"required", true},
                        {"primary_key", true},
                        {"system", true},
                        {"unique", false},
                        {
                            "constraints", {
                                {"min-value", 6},
                                {"max_value", nullptr},
                                {"validator", "@password"},
                                {"default_value", nullptr},
                            }
                        },
                    }
            },
            EntitySchemaField{
                    {
                        {"name", "created"},
                        {"type", "date"},
                        {"required", true},
                        {"primary_key", false},
                        {"system", true},
                        {"unique", false},
                        {
                            "constraints", {
                                {"min-value", nullptr},
                                {"max_value", nullptr},
                                {"validator", nullptr},
                                {"default_value", nullptr},
                            }
                        },
                    }
            },
            EntitySchemaField{
                    {
                        {"name", "updated"},
                        {"type", "date"},
                        {"required", true},
                        {"primary_key", false},
                        {"system", true},
                        {"unique", false},
                        {
                            "constraints", {
                                {"min-value", nullptr},
                                {"max_value", nullptr},
                                {"validator", nullptr},
                                {"default_value", nullptr},
                            }
                        },
                    }
            },
            EntitySchemaField{
                {
                    //User NAME
                    {"name", "name"},
                    {"type", "string"},
                    {"required", true},
                    {"primary_key", false},
                    {"system", true},
                    {"unique", false},
                    {
                        "constraints", {
                            {"min-value", 3},
                            {"max_value", nullptr},
                            {"validator", nullptr},
                            {"default_value", nullptr},
                        }
                    },
                }
            },
            EntitySchemaField{
                {
                    // User EMAIL
                    {"name", "email"},
                    {"type", "string"},
                    {"required", true},
                    {"primary_key", false},
                    {"system", true},
                    {"unique", true},
                    {
                        "constraints", {
                            {"min-value", 5},
                            {"max_value", nullptr},
                            {"validator", "@email"},
                            {"default_value", nullptr},
                        }
                    },
                }
            },
            EntitySchemaField{
                {
                    {"name", "password"},
                    {"type", "string"},
                    {"required", true},
                    {"primary_key", false},
                    {"system", true},
                    {"unique", false},
                    {
                        "constraints", {
                            {"min-value", 6},
                            {"max_value", nullptr},
                            {"validator", "@password"},
                            {"default_value", nullptr},
                        }
                    },
                }
            }
        };
        return _auth_field_schema;
    }

    const std::vector<std::string> &EntitySchemaField::defaultBaseFields() {
        static const std::vector<std::string> _base_fields = {"id", "created", "updated"};
        return _base_fields;
    }

    const std::vector<std::string> &EntitySchemaField::defaultAuthFields() {
        static const std::vector<std::string> _auth_fields = {
            "id", "created", "updated", "name", "email", "password"
        };
        return _auth_fields;
    }

    const std::vector<std::string> &EntitySchemaField::defaultEntityFieldTypes() {
        static const std::vector<std::string> _fieldTypes = {
            "xml", "string", "double", "date", "int8", "uint8",
            "int16", "uint16", "int32", "uint32", "int64", "uint64",
            "blob", "json", "bool", "file", "files"
        };
        return _fieldTypes;
    }
} // mantis
