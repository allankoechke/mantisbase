//
// Created by allan on 18/06/2025.
//
#include <gtest/gtest.h>
#include "mantisbase/core/auth.h"
#include "mantisbase/mantisbase.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include "../test_fixure.h"
#include "../common/test_config.h"

namespace fs = std::filesystem;

class JWTTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Create unique test directory for each test
        testDir = fs::temp_directory_path() / "mantisbase_test_jwt" / mb::generateShortId();
        fs::create_directories(testDir);
        fs::permissions(testDir, fs::perms::owner_all, fs::perm_options::replace);
        
        // Initialize MantisBase for JWT secret key
        config["database"] = "SQLITE";
        config["dataDir"] = testDir.string();
        config["serve"] = {{"port", 0}, {"host", "0.0.0.0"}}; // Port 0 = don't start server
        
        app = &mb::MantisBase::create(config);
    }
    
    void TearDown() override {
        if (app) {
            app->close();
        }
        // Cleanup test directory
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }
    
    fs::path testDir;
    mb::json config;
    mb::MantisBase* app = nullptr;
};

TEST_F(JWTTestFixture, CreateValidToken) {
    const nlohmann::json claims = {{"id", "123"}, {"entity", "users"}};
    std::string token = mb::Auth::createToken(claims, 3600);
    
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
    std::string token = mb::Auth::createToken(claims, 3600);
    
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