#include <gtest/gtest.h>
#include "mantisbase/core/models/validators.h"
#include "mantisbase/core/models/entity.h"
#include "mantisbase/core/exceptions.h"
#include "../common/test_fixture.h"

class ValidatorsTest : public MbAppFixture {
protected:
    static mb::json makeField(const std::string &name, const std::string &type,
                              const mb::json &extra = mb::json::object()) {
        mb::json field = {
            {"name", name},
            {"type", type},
            {"constraints", {
                {"default_value", nullptr},
                {"min_value", nullptr},
                {"max_value", nullptr},
                {"validator", nullptr}
            }}
        };
        for (auto &[key, val] : extra.items()) {
            field[key] = val;
        }
        return field;
    }
};

TEST_F(ValidatorsTest, FindPresetStripsAtPrefix) {
    auto preset = mb::Validators::findPreset("@email");
    ASSERT_TRUE(preset.has_value());
    EXPECT_TRUE(preset->contains("regex"));

    auto preset2 = mb::Validators::findPreset("email");
    ASSERT_TRUE(preset2.has_value());

    EXPECT_FALSE(mb::Validators::findPreset("unknown_preset").has_value());
    EXPECT_FALSE(mb::Validators::findPreset("").has_value());
}

TEST_F(ValidatorsTest, ValidatePresetAcceptsValidEmail) {
    EXPECT_NO_THROW(mb::Validators::validatePreset("email", "user@example.com"));
}

TEST_F(ValidatorsTest, ValidatePresetRejectsInvalidEmail) {
    EXPECT_THROW(mb::Validators::validatePreset("email", "not-an-email"), mb::MantisException);
}

TEST_F(ValidatorsTest, ValidatePresetRejectsUnknownKey) {
    EXPECT_THROW(mb::Validators::validatePreset("unknown", "value"), mb::MantisException);
}

TEST_F(ValidatorsTest, RequiredConstraintCheck) {
    auto field = makeField("name", "string", {{"required", true}});

    EXPECT_TRUE(mb::Validators::requiredConstraintCheck(field, mb::json::object()).has_value());
    EXPECT_FALSE(mb::Validators::requiredConstraintCheck(field, {{"name", "ok"}}).has_value());
    EXPECT_TRUE(mb::Validators::requiredConstraintCheck(field, {{"name", nullptr}}).has_value());
}

TEST_F(ValidatorsTest, MinimumConstraintCheck) {
    auto field = makeField("title", "string");
    field["constraints"]["min_value"] = 3;

    EXPECT_TRUE(mb::Validators::minimumConstraintCheck(field, {{"title", "ab"}}).has_value());
    EXPECT_FALSE(mb::Validators::minimumConstraintCheck(field, {{"title", "abc"}}).has_value());
}

TEST_F(ValidatorsTest, MaximumConstraintCheck) {
    auto field = makeField("count", "int");
    field["constraints"]["max_value"] = 10;

    EXPECT_TRUE(mb::Validators::maximumConstraintCheck(field, {{"count", 11}}).has_value());
    EXPECT_FALSE(mb::Validators::maximumConstraintCheck(field, {{"count", 5}}).has_value());
}

TEST_F(ValidatorsTest, ValidatorConstraintCheckPreset) {
    auto field = makeField("email", "string");
    field["constraints"]["validator"] = "@email";

    // Current implementation returns error string on regex mismatch
    EXPECT_TRUE(mb::Validators::validatorConstraintCheck(field, {{"email", "bad"}}).has_value());
    EXPECT_FALSE(mb::Validators::validatorConstraintCheck(field, {{"email", "good@example.com"}}).has_value());
}

TEST_F(ValidatorsTest, ForeignKeyConstraintCheckUnknownEntity) {
    auto field = makeField("ref_id", "string");
    field["foreign_key"] = {{"entity", "does_not_exist"}, {"field", "id"}};

    auto err = mb::Validators::foreignKeyConstraintCheck(mantis(), field, {{"ref_id", "abc"}});
    ASSERT_TRUE(err.has_value());
    EXPECT_NE(err->find("does_not_exist"), std::string::npos);
}

TEST_F(ValidatorsTest, ForeignKeyConstraintCheckKnownEntity) {
    auto field = makeField("admin_id", "string");
    field["foreign_key"] = {{"entity", "mb_admins"}, {"field", "id"}};

    auto err = mb::Validators::foreignKeyConstraintCheck(mantis(), field, {{"admin_id", "some-id"}});
    EXPECT_FALSE(err.has_value());
}

TEST_F(ValidatorsTest, ValidateTableSchema) {
    EXPECT_TRUE(mb::Validators::validateTableSchema(mb::json::object()).has_value());

    mb::json schema = {
        {"name", "items"},
        {"type", "base"},
        {"fields", mb::json::array({{{"name", "title"}, {"type", "string"}}})}
    };
    EXPECT_FALSE(mb::Validators::validateTableSchema(schema).has_value());

    mb::json bad_view = {{"name", "v"}, {"type", "view"}};
    EXPECT_TRUE(mb::Validators::validateTableSchema(bad_view).has_value());
}

TEST_F(ValidatorsTest, ValidateRequestBodyRequiredField) {
    mb::json schema = {
        {"name", "items"},
        {"type", "base"},
        {"fields", mb::json::array({
            {{"name", "title"}, {"type", "string"}, {"required", true},
             {"constraints", {
                 {"default_value", nullptr},
                 {"min_value", nullptr},
                 {"max_value", nullptr},
                 {"validator", nullptr}
             }}}
        })}
    };
    mb::Entity entity(mantis(), schema);

    auto err = mb::Validators::validateRequestBody(entity, mb::json::object());
    ASSERT_TRUE(err.has_value());
    EXPECT_NE(err->find("title"), std::string::npos);
}
