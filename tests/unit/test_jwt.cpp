//
// Created by allan on 18/06/2025.
//
#include <gtest/gtest.h>
#include "mantisbase/core/auth.h"
#include "mantisbase/mantisbase.h"
#include <nlohmann/json.hpp>

TEST(Auth, CreateValidToken) {
    // Initialize MantisBase for JWT secret key
    mb::json config;
    config["database"] = "SQLITE";
    config["dataDir"] = "/tmp/mantisbase_test_jwt";
    config["serve"] = {{"port", 7076}, {"host", "0.0.0.0"}};
    
    auto& app = mb::MantisBase::create(config);
    
    const nlohmann::json claims = {{"id", "123"}, {"entity", "users"}};
    std::string token = mb::Auth::createToken(claims, 3600);
    
    EXPECT_FALSE(token.empty());
    EXPECT_GT(token.length(), 20); // JWT tokens are typically long
    
    app.close();
}

TEST(Auth, CreateTokenMissingFields) {
    mb::json config;
    config["database"] = "SQLITE";
    config["dataDir"] = "/tmp/mantisbase_test_jwt2";
    config["serve"] = {{"port", 7077}, {"host", "0.0.0.0"}};
    
    auto& app = mb::MantisBase::create(config);
    
    // Missing entity field
    nlohmann::json claims = {{"id", "123"}};
    EXPECT_THROW(mb::Auth::createToken(claims, 3600), std::invalid_argument);
    
    // Missing id field
    claims = {{"entity", "users"}};
    EXPECT_THROW(mb::Auth::createToken(claims, 3600), std::invalid_argument);
    
    app.close();
}

TEST(Auth, VerifyValidToken) {
    mb::json config;
    config["database"] = "SQLITE";
    config["dataDir"] = "/tmp/mantisbase_test_jwt3";
    config["serve"] = {{"port", 7078}, {"host", "0.0.0.0"}};
    
    auto& app = mb::MantisBase::create(config);
    
    const nlohmann::json claims = {{"id", "123"}, {"entity", "users"}};
    std::string token = mb::Auth::createToken(claims, 3600);
    
    EXPECT_FALSE(token.empty());
    
    auto verify_result = mb::Auth::verifyToken(token);
    
    EXPECT_TRUE(verify_result["verified"].get<bool>());
    EXPECT_EQ(verify_result["claims"]["id"].get<std::string>(), "123");
    EXPECT_EQ(verify_result["claims"]["entity"].get<std::string>(), "users");
    EXPECT_TRUE(verify_result["error"].get<std::string>().empty());
    
    app.close();
}

TEST(Auth, VerifyInvalidToken) {
    mb::json config;
    config["database"] = "SQLITE";
    config["dataDir"] = "/tmp/mantisbase_test_jwt4";
    config["serve"] = {{"port", 7079}, {"host", "0.0.0.0"}};
    
    auto& app = mb::MantisBase::create(config);
    
    // Invalid token
    auto verify_result = mb::Auth::verifyToken("invalid.token.here");
    
    EXPECT_FALSE(verify_result["verified"].get<bool>());
    EXPECT_FALSE(verify_result["error"].get<std::string>().empty());
    
    // Empty token
    verify_result = mb::Auth::verifyToken("");
    EXPECT_FALSE(verify_result["verified"].get<bool>());
    
    app.close();
}