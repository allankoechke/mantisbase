//
// Created by allan on 18/06/2025.
//
#include <gtest/gtest.h>
#include "mantis/core/jwt.h"
#include <nlohmann/json.hpp>

TEST(JWT, CreateValidToken) {
    // const nlohmann::json claims = {{"id", "123"}, {"table", "users"}};
    // auto result = mb::JWT::createJWTToken(claims, "test_secret");
    //
    // EXPECT_TRUE(result.at("error").get<std::string>().empty());
    // EXPECT_FALSE(result.at("token").get<std::string>().empty());
}

TEST(JWT, VerifyValidToken) {
    // const nlohmann::json claims = {{"id", "123"}, {"table", "users"}};
    // auto token_result = mb::JWT::createJWTToken(claims, "test_secret");
    //
    // // Verify that the token is not empty
    // EXPECT_TRUE(token_result.at("error").get<std::string>().empty());
    //
    // mb::Log::debug("Token Verification Error? {}", token_result.dump());
    //
    // auto verify_result = mb::JWT::verifyJWTToken(
    //     token_result["token"], "test_secret");
    //
    // EXPECT_TRUE(verify_result.at("error").get<std::string>().empty());
    // EXPECT_EQ(verify_result["id"], "123");
    // EXPECT_EQ(verify_result["table"], "users");
}