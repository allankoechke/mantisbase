//
// Created by allan on 18/06/2025.
//

#include <gtest/gtest.h>
#include <mantisbase/mantis.h>
#include "../common/test_fixture.h"

class DatabaseTest : public MbAppFixture {
};

TEST_F(DatabaseTest, TestDatabaseConnected) {
    EXPECT_TRUE(mantis().db().isConnected());
}

TEST_F(DatabaseTest, CheckSystemSchemaMigrated) {
    auto &mApp = mantis();
    auto admin_entity = mApp.entity("mb_admins");

    EXPECT_EQ(admin_entity.type(), "auth");
    EXPECT_EQ(admin_entity.name(), "mb_admins");
    EXPECT_TRUE(admin_entity.hasApi());
    EXPECT_TRUE(admin_entity.isSystem());

    mb::EntitySchema admin_schema = mb::EntitySchema::fromEntity(mApp, admin_entity);

    EXPECT_TRUE(admin_schema.hasField("id"));
    EXPECT_EQ(admin_schema.field("id").type(), "string");

    EXPECT_TRUE(admin_schema.field("id").isSystem());
    EXPECT_TRUE(admin_schema.field("id").isPrimaryKey());

    EXPECT_TRUE(admin_schema.hasField("updated"));
    EXPECT_EQ(admin_schema.field("updated").type(), "date");

    EXPECT_TRUE(admin_schema.hasField("created"));
    EXPECT_EQ(admin_schema.field("created").type(), "date");

    EXPECT_TRUE(admin_schema.hasField("email"));
    EXPECT_EQ(admin_schema.field("email").type(), "string");

    EXPECT_TRUE(admin_schema.hasField("password"));
    EXPECT_EQ(admin_schema.field("password").type(), "string");
    auto c = admin_schema.field("password").constraints();
    EXPECT_EQ(c["validator"].get<std::string>(), "@password");
}

TEST_F(DatabaseTest, EntityOperations) {
    const auto &mApp = mantis();

    EXPECT_NO_THROW(auto _ = mApp.entity("mb_admins"));
    EXPECT_THROW(auto _ = mApp.entity("mb_tables"), mb::MantisException);

    const auto admin_entity = mApp.entity("mb_admins");
    EXPECT_TRUE(admin_entity.isSystem());
    EXPECT_EQ(admin_entity.type(), "auth");

    EXPECT_TRUE(admin_entity.hasField("id"));
    EXPECT_TRUE(admin_entity.hasField("password"));
    EXPECT_TRUE(admin_entity.hasField("email"));
    EXPECT_TRUE(admin_entity.hasField("name"));
    EXPECT_TRUE(admin_entity.hasField("created"));
    EXPECT_TRUE(admin_entity.hasField("updated"));
}
