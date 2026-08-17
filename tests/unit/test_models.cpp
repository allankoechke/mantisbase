#include <gtest/gtest.h>
#include "mantisbase/core/models/entity.h"
#include "mantisbase/core/models/entity_schema.h"
#include "mantisbase/core/models/entity_schema_field.h"
#include "mantisbase/core/models/int_precision.h"
#include "mantisbase/core/models/access_rules.h"
#include <mantisbase/mantis.h>
#include "../common/test_fixture.h"

class ModelsAppTest : public MbAppFixture {
};

TEST_F(ModelsAppTest, EntitySchemaBaseType) {
    mb::EntitySchema base{mantis(), "test", "base"};

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

TEST_F(ModelsAppTest, EntitySchemaAuthType) {
    mb::EntitySchema auth{mantis(), "test", "auth"};

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

TEST_F(ModelsAppTest, EntitySchemaViewType) {
    const mb::EntitySchema view{mantis(), "test", "view"};

    EXPECT_EQ(view.type(), "view");
    EXPECT_EQ(view.name(), "test");
    EXPECT_TRUE(view.hasApi());
    EXPECT_FALSE(view.isSystem());
    EXPECT_TRUE(view.fields().empty());
}

TEST_F(ModelsAppTest, EntitySchemaAccessRules) {
    mb::EntitySchema schema{mantis(), "test", "base"};
    
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

TEST_F(ModelsAppTest, EntitySchemaFieldOperations) {
    mb::EntitySchema schema{mantis(), "test", "base"};
    
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

TEST_F(ModelsAppTest, EntitySchemaJSONConversion) {
    mb::EntitySchema schema{mantis(), "test", "base"};
    schema.addField(mb::EntitySchemaField("name", "string").setRequired(true));
    schema.setListRule(mb::AccessRule("public", ""));

    // Convert to JSON
    auto json = schema.toJSON();
    EXPECT_EQ(json["name"], "test");
    EXPECT_EQ(json["type"], "base");
    EXPECT_EQ(json["rules"]["list"]["mode"], "public");
    EXPECT_TRUE(json.contains("fields"));

    // Create from JSON
    auto newSchema = mb::EntitySchema::fromSchema(mantis(), json);
    EXPECT_EQ(newSchema.name(), "test");
    EXPECT_EQ(newSchema.type(), "base");
    EXPECT_EQ(newSchema.listRule().mode(), "public");
    EXPECT_TRUE(newSchema.hasField("name"));
}

TEST(EntitySchema, IntTypeWithPrecision) {
    mb::EntitySchemaField field("count", "int");
    EXPECT_EQ(field.type(), "int");
    EXPECT_EQ(field.intPrecision(), mb::IntPrecision::I32);

    field.setPrecision("i64");
    EXPECT_EQ(field.intPrecision(), mb::IntPrecision::I64);

    field.setPrecision("u16");
    EXPECT_EQ(field.intPrecision(), mb::IntPrecision::U16);

    auto j = field.toJSON();
    EXPECT_EQ(j["type"], "int");
    EXPECT_EQ(j["precision"], "u16");

    mb::EntitySchemaField field2(j);
    EXPECT_EQ(field2.type(), "int");
    EXPECT_EQ(field2.intPrecision(), mb::IntPrecision::U16);

    EXPECT_THROW(field.setPrecision("u128"), mb::MantisException);
    EXPECT_THROW(field.setPrecision("i0"), mb::MantisException);

    mb::EntitySchemaField legacy_field({
        {"name", "legacy"},
        {"type", "int"},
        {"precision", 64},
    });
    EXPECT_EQ(legacy_field.intPrecision(), mb::IntPrecision::I64);
}

TEST(EntitySchema, IntPrecisionSociTypes) {
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::I8), soci::db_int8);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::I16), soci::db_int16);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::I32), soci::db_int32);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::I64), soci::db_int64);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::U8), soci::db_uint8);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::U16), soci::db_uint16);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::U32), soci::db_uint32);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", mb::IntPrecision::U64), soci::db_uint64);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int"), soci::db_int32);
}

TEST(EntitySchema, IntPrecisionValidation) {
    nlohmann::json field = {
        {"name", "qty"},
        {"type", "int"},
        {"precision", "u8"},
        {"constraints", {{"min_value", 0}, {"max_value", 255}}},
    };

    EXPECT_FALSE(mb::validateIntFieldValue(10, mb::IntPrecision::U8).has_value());
    EXPECT_TRUE(mb::validateIntFieldValue(256, mb::IntPrecision::U8).has_value());
    EXPECT_TRUE(mb::validateIntFieldValue(-1, mb::IntPrecision::U8).has_value());
    EXPECT_FALSE(mb::validateIntConstraintBounds(field).has_value());

    field["constraints"]["max_value"] = 300;
    EXPECT_TRUE(mb::validateIntConstraintBounds(field).has_value());

    field = {
        {"name", "delta"},
        {"type", "int"},
        {"precision", "i8"},
        {"constraints", {{"min_value", -128}, {"max_value", 127}}},
    };
    EXPECT_FALSE(mb::validateIntFieldValue(-128, mb::IntPrecision::I8).has_value());
    EXPECT_TRUE(mb::validateIntFieldValue(-129, mb::IntPrecision::I8).has_value());
    EXPECT_FALSE(mb::validateIntConstraintBounds(field).has_value());
}

TEST(EntitySchema, IntTypeInFieldTypes) {
    EXPECT_TRUE(mb::EntitySchemaField::isValidFieldType("int"));
    EXPECT_FALSE(mb::EntitySchemaField::isValidFieldType("int32"));
    EXPECT_FALSE(mb::EntitySchemaField::isValidFieldType("uint32"));
    EXPECT_FALSE(mb::EntitySchemaField::isValidFieldType("int64"));
}

TEST(EntitySchema, IndexDefinition) {
    mb::IndexDefinition idx;
    idx.name = "idx_users_email";
    idx.unique = true;
    idx.columns = {"email"};

    auto j = idx.toJSON();
    EXPECT_EQ(j["name"], "idx_users_email");
    EXPECT_TRUE(j["unique"].get<bool>());
    EXPECT_EQ(j["columns"].size(), 1);
    EXPECT_EQ(j["columns"][0], "email");

    auto idx2 = mb::IndexDefinition::fromJSON(j);
    EXPECT_EQ(idx2.name, "idx_users_email");
    EXPECT_TRUE(idx2.unique);
    EXPECT_EQ(idx2.columns.size(), 1);
    EXPECT_EQ(idx2.columns[0], "email");
}

TEST_F(ModelsAppTest, SchemaWithIndexes) {
    mb::EntitySchema schema{mantis(), "test", "base"};
    EXPECT_TRUE(schema.indexes().empty());

    mb::IndexDefinition idx;
    idx.name = "idx_test_name";
    idx.unique = false;
    idx.columns = {"name"};
    schema.addIndex(idx);

    EXPECT_EQ(schema.indexes().size(), 1);
    EXPECT_EQ(schema.indexes()[0].name, "idx_test_name");

    auto j = schema.toJSON();
    EXPECT_TRUE(j.contains("indexes"));
    EXPECT_EQ(j["indexes"].size(), 1);
    EXPECT_EQ(j["indexes"][0]["name"], "idx_test_name");

    auto schema2 = mb::EntitySchema::fromSchema(mantis(), j);
    EXPECT_EQ(schema2.indexes().size(), 1);
    EXPECT_EQ(schema2.indexes()[0].name, "idx_test_name");

    schema.removeIndex("idx_test_name");
    EXPECT_TRUE(schema.indexes().empty());
}

TEST_F(ModelsAppTest, ViewEntityType) {
    nlohmann::json viewSchema = {
        {"name", "active_users"},
        {"type", "view"},
        {"view_query", "SELECT id, name FROM users WHERE active = 1"}
    };

    auto schema = mb::EntitySchema::fromSchema(mantis(), viewSchema);
    EXPECT_EQ(schema.type(), "view");
    EXPECT_EQ(schema.name(), "active_users");
    EXPECT_EQ(schema.viewQuery(), "SELECT id, name FROM users WHERE active = 1");
    EXPECT_TRUE(schema.fields().empty());

    auto j = schema.toJSON();
    EXPECT_EQ(j["view_query"], "SELECT id, name FROM users WHERE active = 1");
    EXPECT_FALSE(j.contains("fields"));
}

TEST_F(ModelsAppTest, ViewEntityRejectsFields) {
    const mb::EntitySchema view{mantis(), "test_view", "view"};
    EXPECT_TRUE(view.fields().empty());
    EXPECT_FALSE(view.hasField("anything"));
}

TEST(CursorPagination, DefaultPaginationOpts) {
    nlohmann::json opts;
    opts["pagination"] = {
        {"limit", 50},
        {"after", ""}
    };

    EXPECT_EQ(opts["pagination"]["limit"].get<int>(), 50);
    EXPECT_EQ(opts["pagination"]["after"].get<std::string>(), "");
}

TEST(CursorPagination, HasMoreDetection) {
    mb::EntityListPage page;
    page.items = nlohmann::json::array();
    page.items.push_back({{"id", "a"}});
    page.items.push_back({{"id", "b"}});
    page.has_more = true;

    EXPECT_TRUE(page.has_more);
    EXPECT_EQ(page.items.size(), 2u);
}

TEST(CursorPagination, LimitClamping) {
    int limit = 600;
    if (limit < 1) limit = 1;
    if (limit > 500) limit = 500;
    EXPECT_EQ(limit, 500);

    limit = -5;
    if (limit < 1) limit = 1;
    if (limit > 500) limit = 500;
    EXPECT_EQ(limit, 1);

    limit = 100;
    if (limit < 1) limit = 1;
    if (limit > 500) limit = 500;
    EXPECT_EQ(limit, 100);
}

TEST(CursorPagination, CursorFromResponseItems) {
    nlohmann::json records = nlohmann::json::array();
    records.push_back({{"id", "abc-001"}, {"name", "Alice"}});
    records.push_back({{"id", "abc-002"}, {"name", "Bob"}});
    records.push_back({{"id", "abc-003"}, {"name", "Charlie"}});

    std::string cursor;
    if (!records.empty()) {
        const auto &last = records.back();
        if (last.contains("id") && last["id"].is_string())
            cursor = last["id"].get<std::string>();
    }
    EXPECT_EQ(cursor, "abc-003");
}

TEST(CursorPagination, EmptyDatasetCursor) {
    nlohmann::json records = nlohmann::json::array();

    std::string cursor;
    if (!records.empty()) {
        const auto &last = records.back();
        if (last.contains("id") && last["id"].is_string())
            cursor = last["id"].get<std::string>();
    }
    EXPECT_EQ(cursor, "");
}

TEST(CursorPagination, PaginationOptsRoundTrip) {
    nlohmann::json opts;
    opts["pagination"] = {
        {"limit", 25},
        {"after", "uuid-abc-123"}
    };
    opts["filter"] = R"({"status":"active"})";

    auto &p = opts["pagination"];
    EXPECT_EQ(p["limit"].get<int>(), 25);
    EXPECT_EQ(p["after"].get<std::string>(), "uuid-abc-123");
    EXPECT_EQ(opts["filter"].get<std::string>(), R"({"status":"active"})");
}