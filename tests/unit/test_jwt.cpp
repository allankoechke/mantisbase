//
// Created by allan on 18/06/2025.
//
#include <gtest/gtest.h>
#include "mantisbase/core/auth.h"
#include "mantisbase/mantisbase.h"
#include <nlohmann/json.hpp>
#include "../test_fixure.h"
#include "../common/test_config.h"

class JWTTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Use the existing MantisBase instance from the test environment
        // JWT tests use the shared instance initialized by MantisBaseTestEnvironment
        // This ensures all tests use the same MantisBase singleton
        app = &mb::MantisBase::instance();
    }
    
    void TearDown() override {
        // Don't close the shared instance - let the test environment handle cleanup
        // The MantisBase instance is managed by MantisBaseTestEnvironment
    }
    
    mb::MantisBase* app = nullptr;
};

TEST_F(JWTTestFixture, CreateValidToken) {
    const nlohmann::json claims = {{"id", "123"}, {"entity", "users"}};
    const std::string token = mb::Auth::createToken(claims, 3600);
    
    EXPECT_FALSE(token.empty());
    EXPECT_GT(token.length(), 20); // JWT tokens are typically long
}

TEST_F(JWTTestFixture, CreateTokenMissingFields) {
    // Missing entity field
    nlohmann::json claims = {{"id", "123"}};
    EXPECT_THROW(mb::Auth::createToken(claims, 3600), std::invalid_argument);
    
    // Missing id field
    claims = {{"entity", "users"}};
    EXPECT_THROW(mb::Auth::createToken(claims, 3600), std::invalid_argument);
}

TEST_F(JWTTestFixture, VerifyValidToken) {
    const nlohmann::json claims = {{"id", "123"}, {"entity", "users"}};
    const std::string token = mb::Auth::createToken(claims, 3600);
    
    EXPECT_FALSE(token.empty());
    
    auto verify_result = mb::Auth::verifyToken(token);
    
    EXPECT_TRUE(verify_result["verified"].get<bool>());
    EXPECT_EQ(verify_result["claims"]["id"].get<std::string>(), "123");
    EXPECT_EQ(verify_result["claims"]["entity"].get<std::string>(), "users");
    EXPECT_TRUE(verify_result["error"].get<std::string>().empty());
}

TEST_F(JWTTestFixture, VerifyInvalidToken) {
    // Invalid token
    auto verify_result = mb::Auth::verifyToken("invalid.token.here");
    
    EXPECT_FALSE(verify_result["verified"].get<bool>());
    EXPECT_FALSE(verify_result["error"].get<std::string>().empty());
    
    // Empty token
    verify_result = mb::Auth::verifyToken("");
    EXPECT_FALSE(verify_result["verified"].get<bool>());
}