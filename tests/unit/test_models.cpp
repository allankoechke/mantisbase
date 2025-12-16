#include <gtest/gtest.h>
#include "mantisbase/core/models/entity.h"
#include "mantisbase/core/models/entity_schema.h"
#include "mantisbase/core/models/entity_schema_field.h"
#include "mantisbase/core/models/access_rules.h"
#include <mantisbase/mantis.h>

TEST(EntitySchema, EntitySchemaBaseType) {
    // Base field type
    mb::EntitySchema base{"test", "base"};

    EXPECT_EQ(base.type(), "base");
    EXPECT_EQ(base.name(), "test");
    EXPECT_TRUE(base.hasApi());
    EXPECT_FALSE(base.isSystem());
    
    // Check default access rules (empty mode = admin only)
    EXPECT_EQ(base.listRule().mode(), "");
    EXPECT_EQ(base.listRule().expr(), "");
    EXPECT_EQ(base.getRule().mode(), "");
    EXPECT_EQ(base.addRule().mode(), "");
    EXPECT_EQ(base.updateRule().mode(), "");
    EXPECT_EQ(base.deleteRule().mode(), "");
    
    EXPECT_TRUE(base.hasField("id"));
    EXPECT_EQ(base.field("id").type(), "string");
    EXPECT_TRUE(base.field("id").isSystem());
    EXPECT_TRUE(base.field("id").isPrimaryKey());
    EXPECT_TRUE(base.hasField("updated"));
    EXPECT_EQ(base.field("updated").type(), "date");
    EXPECT_TRUE(base.hasField("created"));
    EXPECT_EQ(base.field("created").type(), "date");
}

TEST(EntitySchema, EntitySchemaAuthType) {
    // Auth Type
    mb::EntitySchema auth{"test", "auth"};

    EXPECT_EQ(auth.type(), "auth");
    EXPECT_EQ(auth.name(), "test");
    EXPECT_TRUE(auth.hasApi());
    EXPECT_FALSE(auth.isSystem());
    
    EXPECT_TRUE(auth.hasField("id"));
    EXPECT_EQ(auth.field("id").type(), "string");
    EXPECT_TRUE(auth.field("id").isSystem());
    EXPECT_TRUE(auth.field("id").isPrimaryKey());
    EXPECT_TRUE(auth.hasField("updated"));
    EXPECT_EQ(auth.field("updated").type(), "date");
    EXPECT_TRUE(auth.hasField("created"));
    EXPECT_EQ(auth.field("created").type(), "date");
    EXPECT_TRUE(auth.hasField("name"));
    EXPECT_EQ(auth.field("name").type(), "string");

    EXPECT_TRUE(auth.hasField("email"));
    EXPECT_EQ(auth.field("email").type(), "string");

    EXPECT_TRUE(auth.hasField("password"));
    EXPECT_EQ(auth.field("password").type(), "string");
    auto c = auth.field("password").constraints();
    EXPECT_EQ(c["validator"].get<std::string>(), "@password");
}

TEST(EntitySchema, EntitySchemaViewType) {
    // View Type
    mb::EntitySchema view{"test", "view"};

    EXPECT_EQ(view.type(), "view");
    EXPECT_EQ(view.name(), "test");
    EXPECT_TRUE(view.hasApi());
    EXPECT_FALSE(view.isSystem());
    EXPECT_TRUE(view.fields().empty());
}

TEST(EntitySchema, EntitySchemaAccessRules) {
    mb::EntitySchema schema{"test", "base"};
    
    // Set access rules with different modes
    schema.setListRule(mb::AccessRule("public", ""));
    schema.setGetRule(mb::AccessRule("auth", ""));
    schema.setAddRule(mb::AccessRule("custom", "auth.id != \"\""));
    schema.setUpdateRule(mb::AccessRule("", "")); // Admin only
    schema.setDeleteRule(mb::AccessRule("custom", "auth.entity == \"mb_admins\""));
    
    EXPECT_EQ(schema.listRule().mode(), "public");
    EXPECT_EQ(schema.listRule().expr(), "");
    
    EXPECT_EQ(schema.getRule().mode(), "auth");
    EXPECT_EQ(schema.getRule().expr(), "");
    
    EXPECT_EQ(schema.addRule().mode(), "custom");
    EXPECT_EQ(schema.addRule().expr(), "auth.id != \"\"");
    
    EXPECT_EQ(schema.updateRule().mode(), "");
    EXPECT_EQ(schema.updateRule().expr(), "");
    
    EXPECT_EQ(schema.deleteRule().mode(), "custom");
    EXPECT_EQ(schema.deleteRule().expr(), "auth.entity == \"mb_admins\"");
}

TEST(EntitySchema, EntitySchemaFieldOperations) {
    mb::EntitySchema schema{"test", "base"};
    
    // Add custom fields
    mb::EntitySchemaField nameField("name", "string");
    nameField.setRequired(true);
    schema.addField(nameField);
    
    mb::EntitySchemaField emailField("email", "string");
    emailField.setIsUnique(true);
    schema.addField(emailField);
    
    EXPECT_TRUE(schema.hasField("name"));
    EXPECT_TRUE(schema.hasField("email"));
    EXPECT_TRUE(schema.field("name").required());
    EXPECT_TRUE(schema.field("email").isUnique());
    
    // Remove field
    schema.removeField("name");
    EXPECT_FALSE(schema.hasField("name"));
    EXPECT_TRUE(schema.hasField("email"));
}

TEST(EntitySchema, EntitySchemaJSONConversion) {
    mb::EntitySchema schema{"test", "base"};
    schema.addField(mb::EntitySchemaField("name", "string").setRequired(true));
    schema.setListRule(mb::AccessRule("public", ""));
    
    // Convert to JSON
    auto json = schema.toJSON();
    EXPECT_EQ(json["name"], "test");
    EXPECT_EQ(json["type"], "base");
    EXPECT_EQ(json["rules"]["list"]["mode"], "public");
    EXPECT_TRUE(json.contains("fields"));
    
    // Create from JSON
    auto newSchema = mb::EntitySchema::fromSchema(json);
    EXPECT_EQ(newSchema.name(), "test");
    EXPECT_EQ(newSchema.type(), "base");
    EXPECT_EQ(newSchema.listRule().mode(), "public");
    EXPECT_TRUE(newSchema.hasField("name"));
}