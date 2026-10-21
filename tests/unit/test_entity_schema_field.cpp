#include <gtest/gtest.h>

#include "mantisbase/core/models/entity_schema.h"
#include "mantisbase/core/models/entity_schema_field.h"

TEST(MbEntitySchemaField, BasicConstructor) {
    mb::MbEntitySchemaField field("name", "string");
    
    EXPECT_EQ(field.name(), "name");
    EXPECT_EQ(field.type(), "string");
    EXPECT_FALSE(field.required());
    EXPECT_FALSE(field.isPrimaryKey());
    EXPECT_FALSE(field.isSystem());
    EXPECT_FALSE(field.isUnique());
}

TEST(MbEntitySchemaField, SetRequired) {
    mb::MbEntitySchemaField field("email", "string");
    
    EXPECT_FALSE(field.required());
    field.setRequired(true);
    EXPECT_TRUE(field.required());
    
    field.setRequired(false);
    EXPECT_FALSE(field.required());
}

TEST(MbEntitySchemaField, SetPrimaryKey) {
    mb::MbEntitySchemaField field("id", "string");
    
    EXPECT_FALSE(field.isPrimaryKey());
    field.setIsPrimaryKey(true);
    EXPECT_TRUE(field.isPrimaryKey());
}

TEST(MbEntitySchemaField, SetSystem) {
    mb::MbEntitySchemaField field("created", "date");
    
    EXPECT_FALSE(field.isSystem());
    field.setIsSystem(true);
    EXPECT_TRUE(field.isSystem());
}

TEST(MbEntitySchemaField, SetUnique) {
    mb::MbEntitySchemaField field("email", "string");
    
    EXPECT_FALSE(field.isUnique());
    field.setIsUnique(true);
    EXPECT_TRUE(field.isUnique());
}

TEST(MbEntitySchemaField, Constraints) {
    mb::MbEntitySchemaField field("password", "string");
    
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

TEST(MbEntitySchemaField, ToJSON) {
    mb::MbEntitySchemaField field("name", "string");
    field.setRequired(true);
    field.setIsUnique(true);
    
    auto json = field.toJSON();
    
    EXPECT_EQ(json["name"], "name");
    EXPECT_EQ(json["type"], "string");
    EXPECT_TRUE(json["required"].get<bool>());
    EXPECT_TRUE(json["unique"].get<bool>());
}

TEST(MbEntitySchemaField, UpdateWith) {
    mb::MbEntitySchemaField field("name", "string");
    
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

TEST(MbEntitySchemaField, DefaultBaseFields) {
    auto fields = mb::MbEntitySchema::defaultBaseFieldsSchema();
    
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

TEST(MbEntitySchemaField, DefaultAuthFields) {
    auto fields = mb::MbEntitySchema::defaultAuthFieldsSchema();
    
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

TEST(MbEntitySchemaField, ValidFieldTypes) {
    EXPECT_TRUE(mb::MbEntitySchemaField::isValidFieldType("string"));
    EXPECT_TRUE(mb::MbEntitySchemaField::isValidFieldType("date"));
    EXPECT_TRUE(mb::MbEntitySchemaField::isValidFieldType("bool"));
    EXPECT_TRUE(mb::MbEntitySchemaField::isValidFieldType("file"));
    EXPECT_TRUE(mb::MbEntitySchemaField::isValidFieldType("files"));
    EXPECT_TRUE(mb::MbEntitySchemaField::isValidFieldType("int"));
    EXPECT_TRUE(mb::MbEntitySchemaField::isValidFieldType("json"));
    
    EXPECT_FALSE(mb::MbEntitySchemaField::isValidFieldType("invalid"));
    EXPECT_FALSE(mb::MbEntitySchemaField::isValidFieldType("number"));
    EXPECT_FALSE(mb::MbEntitySchemaField::isValidFieldType("xml"));
    EXPECT_FALSE(mb::MbEntitySchemaField::isValidFieldType("blob"));
    EXPECT_FALSE(mb::MbEntitySchemaField::isValidFieldType(""));
}

