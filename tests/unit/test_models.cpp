#include <gtest/gtest.h>
#include "mantisbase/core/models/entity.h"
#include "mantisbase/core/models/entity_schema.h"
#include "mantisbase/core/models/entity_schema_field.h"
#include <mantisbase/mantis.h>

TEST(EntitySchema, EntitySchemaBaseType) {
    // Base field type
    mb::EntitySchema base{"test", "base"};

    EXPECT_EQ(base.type(), "base");
    EXPECT_EQ(base.name(), "test");
    EXPECT_TRUE(base.hasApi());
    EXPECT_FALSE(base.isSystem());
    // EXPECT_EQ(base.listRule(), "");
    // EXPECT_EQ(base.getRule(), "");
    // EXPECT_EQ(base.addRule(), "");
    // EXPECT_EQ(base.updateRule(), "");
    // EXPECT_EQ(base.deleteRule(), "");
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
    // EXPECT_EQ(auth.listRule(), "");
    // EXPECT_EQ(auth.getRule(), "");
    // EXPECT_EQ(auth.addRule(), "");
    // EXPECT_EQ(auth.updateRule(), "");
    // EXPECT_EQ(auth.deleteRule(), "");
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
    // EXPECT_EQ(view.listRule(), "");
    // EXPECT_EQ(view.getRule(), "");
    // EXPECT_EQ(view.addRule(), "");
    // EXPECT_EQ(view.updateRule(), "");
    // EXPECT_EQ(view.deleteRule(), "");
    EXPECT_TRUE(view.fields().empty());
}