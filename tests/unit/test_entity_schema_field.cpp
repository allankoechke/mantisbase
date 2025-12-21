#include <gtest/gtest.h>

#include "mantisbase/core/models/entity_schema.h"
#include "mantisbase/core/models/entity_schema_field.h"

TEST(EntitySchemaField, BasicConstructor) {
    mb::EntitySchemaField field("name", "string");
    
    EXPECT_EQ(field.name(), "name");
    EXPECT_EQ(field.type(), "string");
    EXPECT_FALSE(field.required());
    EXPECT_FALSE(field.isPrimaryKey());
    EXPECT_FALSE(field.isSystem());
    EXPECT_FALSE(field.isUnique());
}

TEST(EntitySchemaField, SetRequired) {
    mb::EntitySchemaField field("email", "string");
    
    EXPECT_FALSE(field.required());
    field.setRequired(true);
    EXPECT_TRUE(field.required());
    
    field.setRequired(false);
    EXPECT_FALSE(field.required());
}

TEST(EntitySchemaField, SetPrimaryKey) {
    mb::EntitySchemaField field("id", "string");
    
    EXPECT_FALSE(field.isPrimaryKey());
    field.setIsPrimaryKey(true);
    EXPECT_TRUE(field.isPrimaryKey());
}

TEST(EntitySchemaField, SetSystem) {
    mb::EntitySchemaField field("created", "date");
    
    EXPECT_FALSE(field.isSystem());
    field.setIsSystem(true);
    EXPECT_TRUE(field.isSystem());
}

TEST(EntitySchemaField, SetUnique) {
    mb::EntitySchemaField field("email", "string");
    
    EXPECT_FALSE(field.isUnique());
    field.setIsUnique(true);
    EXPECT_TRUE(field.isUnique());
}

TEST(EntitySchemaField, Constraints) {
    mb::EntitySchemaField field("password", "string");
    
    auto constraints = field.constraints();
    EXPECT_TRUE(constraints.is_object());
    
    // Add constraint
    nlohmann::json newConstraints = {
        {"validator", "@password"},
        {"min_value", 8}
    };
    field.setConstraints(newConstraints);
    
    auto updated = field.constraints();
    EXPECT_EQ(updated["validator"], "@password");
    EXPECT_EQ(updated["min_value"], 8);
}

TEST(EntitySchemaField, ToJSON) {
    mb::EntitySchemaField field("name", "string");
    field.setRequired(true);
    field.setIsUnique(true);
    
    auto json = field.toJSON();
    
    EXPECT_EQ(json["name"], "name");
    EXPECT_EQ(json["type"], "string");
    EXPECT_TRUE(json["required"].get<bool>());
    EXPECT_TRUE(json["unique"].get<bool>());
}

TEST(EntitySchemaField, UpdateWith) {
    mb::EntitySchemaField field("name", "string");
    
    nlohmann::json updates = {
        {"required", true},
        {"unique", true},
        {"type", "json"}
    };
    
    field.updateWith(updates);
    
    EXPECT_TRUE(field.required());
    EXPECT_TRUE(field.isUnique());
    EXPECT_EQ(field.type(), "json");
}

TEST(EntitySchemaField, DefaultBaseFields) {
    auto fields = mb::EntitySchema::defaultBaseFieldsSchema();
    
    EXPECT_FALSE(fields.empty());
    
    // Check for id field
    bool hasId = false;
    for (const auto& field : fields) {
        if (field.name() == "id") {
            hasId = true;
            EXPECT_TRUE(field.isPrimaryKey());
            EXPECT_TRUE(field.isSystem());
            break;
        }
    }
    EXPECT_TRUE(hasId);
}

TEST(EntitySchemaField, DefaultAuthFields) {
    auto fields = mb::EntitySchema::defaultAuthFieldsSchema();
    
    EXPECT_FALSE(fields.empty());
    
    // Check for email and password fields
    bool hasEmail = false;
    bool hasPassword = false;
    for (const auto& field : fields) {
        if (field.name() == "email") {
            hasEmail = true;
        }
        if (field.name() == "password") {
            hasPassword = true;
        }
    }
    EXPECT_TRUE(hasEmail);
    EXPECT_TRUE(hasPassword);
}

TEST(EntitySchemaField, ValidFieldTypes) {
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("string"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("date"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("bool"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("file"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("files"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("int8"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("uint8"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("int16"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("uint16"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("int32"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("uint32"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("int64"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("uint64"));
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("json"));
    
    EXPECT_FALSE(mb::EntitySchemaField::isValidFieldType("invalid"));
    EXPECT_FALSE(mb::EntitySchemaField::isValidFieldType("number"));
    EXPECT_FALSE(mb::EntitySchemaField::isValidFieldType(""));
}

