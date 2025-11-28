//
// Created by allan on 18/06/2025.
//

#include <gtest/gtest.h>
#include <mantisbase/mantis.h>
#include "../test_fixure.h"

TEST(DatabaseTest, TestDatabaseConnected) {
    EXPECT_TRUE(TestFixture::app().db().isConnected());
}

TEST(DatabaseTest, CheckSystemSchemaMigrated) {
    auto& mApp = TestFixture::app();
    auto admin_entity = mApp.entity("_admins");

    EXPECT_EQ(admin_entity.type(), "auth");
    EXPECT_EQ(admin_entity.name(), "_admins");
    EXPECT_TRUE(admin_entity.hasApi());
    EXPECT_TRUE(admin_entity.isSystem());
    EXPECT_EQ(admin_entity.listRule(), "");
    EXPECT_EQ(admin_entity.getRule(), "");
    EXPECT_EQ(admin_entity.addRule(), "");
    EXPECT_EQ(admin_entity.updateRule(), "");
    EXPECT_EQ(admin_entity.deleteRule(), "");

    // Create schema, check fields
    mantis::EntitySchema admin_schema = mantis::EntitySchema::fromEntity(admin_entity);

    EXPECT_TRUE(admin_schema.hasField("id"));
    EXPECT_EQ(admin_schema.field("id").type(), "string");

    EXPECT_TRUE(admin_schema.field("id").isSystem());
    EXPECT_TRUE(admin_schema.field("id").isPrimaryKey());

    EXPECT_TRUE(admin_schema.hasField("updated"));
    EXPECT_EQ(admin_schema.field("updated").type(), "date");

    EXPECT_TRUE(admin_schema.hasField("created"));
    EXPECT_EQ(admin_schema.field("created").type(), "date");

    // Name field should have been deleted ...
    // EXPECT_FALSE(admin_schema.hasField("name"));

    EXPECT_TRUE(admin_schema.hasField("email"));
    EXPECT_EQ(admin_schema.field("email").type(), "string");

    EXPECT_TRUE(admin_schema.hasField("password"));
    EXPECT_EQ(admin_schema.field("password").type(), "string");
    auto c = admin_schema.field("password").constraints();
    EXPECT_EQ(c["validator"].get<std::string>(), "@password");
}