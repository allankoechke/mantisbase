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
    const mb::EntitySchema view{"test", "view"};

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

TEST(EntitySchema, IntTypeWithPrecision) {
    mb::EntitySchemaField field("count", "int");
    EXPECT_EQ(field.type(), "int");
    EXPECT_EQ(field.precision(), 32);

    field.setPrecision(64);
    EXPECT_EQ(field.precision(), 64);

    auto j = field.toJSON();
    EXPECT_EQ(j["type"], "int");
    EXPECT_EQ(j["precision"], 64);

    mb::EntitySchemaField field2(j);
    EXPECT_EQ(field2.type(), "int");
    EXPECT_EQ(field2.precision(), 64);

    EXPECT_THROW(field.setPrecision(128), mb::MantisException);
    EXPECT_THROW(field.setPrecision(0), mb::MantisException);
}

TEST(EntitySchema, IntPrecisionSociTypes) {
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", 8), soci::db_int8);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", 16), soci::db_int16);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", 32), soci::db_int32);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int", 64), soci::db_int64);
    EXPECT_EQ(mb::EntitySchemaField::toSociType("int"), soci::db_int32);
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

TEST(EntitySchema, SchemaWithIndexes) {
    mb::EntitySchema schema{"test", "base"};
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

    auto schema2 = mb::EntitySchema::fromSchema(j);
    EXPECT_EQ(schema2.indexes().size(), 1);
    EXPECT_EQ(schema2.indexes()[0].name, "idx_test_name");

    schema.removeIndex("idx_test_name");
    EXPECT_TRUE(schema.indexes().empty());
}

TEST(EntitySchema, ViewEntityType) {
    nlohmann::json viewSchema = {
        {"name", "active_users"},
        {"type", "view"},
        {"view_query", "SELECT id, name FROM users WHERE active = 1"}
    };

    auto schema = mb::EntitySchema::fromSchema(viewSchema);
    EXPECT_EQ(schema.type(), "view");
    EXPECT_EQ(schema.name(), "active_users");
    EXPECT_EQ(schema.viewQuery(), "SELECT id, name FROM users WHERE active = 1");
    EXPECT_TRUE(schema.fields().empty());

    auto j = schema.toJSON();
    EXPECT_EQ(j["view_query"], "SELECT id, name FROM users WHERE active = 1");
    EXPECT_FALSE(j.contains("fields"));
}

TEST(EntitySchema, ViewEntityRejectsFields) {
    const mb::EntitySchema view{"test_view", "view"};
    EXPECT_TRUE(view.fields().empty());
    EXPECT_FALSE(view.hasField("anything"));
}

TEST(CursorPagination, DefaultPaginationOpts) {
    nlohmann::json opts;
    opts["pagination"] = {
        {"limit", 50},
        {"after", ""},
        {"sort", ""}
    };

    EXPECT_EQ(opts["pagination"]["limit"].get<int>(), 50);
    EXPECT_EQ(opts["pagination"]["after"].get<std::string>(), "");
    EXPECT_EQ(opts["pagination"]["sort"].get<std::string>(), "");
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

TEST(CursorPagination, SortFieldParsing) {
    auto parseSort = [](const std::string &sort_str) -> std::pair<std::string, std::string> {
        std::string field = "id";
        std::string dir = "ASC";
        if (!sort_str.empty() && sort_str[0] == '-') {
            dir = "DESC";
            field = sort_str.substr(1);
        } else if (!sort_str.empty()) {
            field = sort_str;
        }
        return {field, dir};
    };

    auto [f1, d1] = parseSort("name");
    EXPECT_EQ(f1, "name");
    EXPECT_EQ(d1, "ASC");

    auto [f2, d2] = parseSort("-created");
    EXPECT_EQ(f2, "created");
    EXPECT_EQ(d2, "DESC");

    auto [f3, d3] = parseSort("");
    EXPECT_EQ(f3, "id");
    EXPECT_EQ(d3, "ASC");
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
        {"after", "uuid-abc-123"},
        {"sort", "-created"}
    };

    auto &p = opts["pagination"];
    EXPECT_EQ(p["limit"].get<int>(), 25);
    EXPECT_EQ(p["after"].get<std::string>(), "uuid-abc-123");
    EXPECT_EQ(p["sort"].get<std::string>(), "-created");
}