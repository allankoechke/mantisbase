//
// Created by allan on 18/06/2025.
//
#include <gtest/gtest.h>
#include "mantisbase/core/auth.h"
#include "mantisbase/mantisbase.h"
#include <nlohmann/json.hpp>
#include "../common/test_fixture.h"
#include "../common/test_config.h"

class JWTTestFixture : public MbAppFixture {
};

TEST_F(JWTTestFixture, CreateValidToken) {
    const nlohmann::json claims = {{"id", "123"}, {"entity", "users"}};
    const std::string token = mantis().auth().createToken(claims, 3600);

    EXPECT_FALSE(token.empty());
    EXPECT_GT(token.length(), 20);
}

TEST_F(JWTTestFixture, CreateTokenMissingFields) {
    nlohmann::json claims = {{"id", "123"}};
    EXPECT_THROW(mantis().auth().createToken(claims, 3600), std::invalid_argument);

    claims = {{"entity", "users"}};
    EXPECT_THROW(mantis().auth().createToken(claims, 3600), std::invalid_argument);
}

TEST_F(JWTTestFixture, VerifyValidToken) {
    const nlohmann::json claims = {{"id", "123"}, {"entity", "users"}};
    const std::string token = mantis().auth().createToken(claims, 3600);

    EXPECT_FALSE(token.empty());

    auto verify_result = mantis().auth().verifyToken(token);

    EXPECT_TRUE(verify_result["verified"].get<bool>());
    EXPECT_EQ(verify_result["claims"]["id"].get<std::string>(), "123");
    EXPECT_EQ(verify_result["claims"]["entity"].get<std::string>(), "users");
    EXPECT_TRUE(verify_result["error"].get<std::string>().empty());
}

TEST_F(JWTTestFixture, VerifyInvalidToken) {
    auto verify_result = mantis().auth().verifyToken("invalid.token.here");

    EXPECT_FALSE(verify_result["verified"].get<bool>());
    EXPECT_FALSE(verify_result["error"].get<std::string>().empty());

    verify_result = mantis().auth().verifyToken("");
    EXPECT_FALSE(verify_result["verified"].get<bool>());
}
