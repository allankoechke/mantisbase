#include "../../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/mantisbase.h"
// #include "../../../include/mantis/core/settings_mgr.h"

#include <cstring>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

namespace mb
{
    std::string Auth::createToken(const json& claims_params, const int timeout)
    {
        // Get signing key for JWT ...
        const auto secretKey = MantisBase::jwtSecretKey();
        if (claims_params.empty() || !claims_params.contains("id") || !claims_params.contains("entity"))
        {
            throw std::invalid_argument("Missing `id` and/or `entity` fields in token claims.");
        }

        // Give access token based on login type, `admin` or `user`
        const auto& config = json::object(); // MantisBase::instance().settings().configs();
        const int expiry_t = timeout > 0
                                 ? timeout // Use `timeout` value if provided
                                 : claims_params.at("entity").get<std::string>() == "mb_admins"
                                 ? config.value("adminSessionTimeout", 1 * 60 * 60) // admins
                                 : config.value("sessionTimeout", 24 * 60 * 60); // users

        const auto time = jwt::date::clock::now();
        auto token_builder = jwt::create()
                             .set_type("JWT")
                             .set_issued_at(time)
                             .set_not_before(time)
                             .set_expires_at(time + std::chrono::seconds(expiry_t));

        // Add JWT Issuer if enabled
        if (config.value("jwtEnableSetIssuer", false))
        {
            token_builder.set_issuer(config.at("appName").get<std::string>());
        }
        // Add JWT audience if enabled
        if (config.value("jwtEnableSetAudience", false))
        {
            token_builder.set_audience(config.at("baseUrl").get<std::string>());
        }

        for (const auto& [key, value] : claims_params.items())
        {
            token_builder.set_payload_claim(key, value);
        }

        const std::string token = token_builder.sign(jwt::algorithm::hs256{secretKey});
        return token;
    }

    json Auth::verifyToken(const std::string& token)
    {
        // Response Object
        json result;
        result["claims"] = json::object();
        result["error"] = "";
        result["verified"] = false;

        try
        {
            // Decode the token
            const auto decoded = jwt::decode(token);
            const auto secretKey = MantisBase::jwtSecretKey();

            // Create verifier with your validation rules
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secretKey});

            const auto& config = json::object(); // MantisBase::instance().settings().configs();
            // Add JWT Issuer if enabled
            if (config.value("jwtEnableSetIssuer", false))
            {
                verifier.with_issuer(config.at("appName").get<std::string>());
            }
            // Add JWT audience if enabled
            if (config.value("jwtEnableSetAudience", false))
            {
                verifier.with_audience(config.at("baseUrl").get<std::string>());
            }

            // Verify with error_code to capture errors
            std::error_code ec;
            verifier.verify(decoded, ec);

            if (ec)
            {
                logger::trace("Token verification failed: {}", ec.message());
                result["error"] = ec.message();
                return result;
            }

            json claims;
            // Validation succeeded - extract all claims
            for (auto& [key, value] : decoded.get_payload_json())
            {
                claims[key] = value;
            }

            // Enforce existence of `id` and `table` claims
            if (!claims.contains("id") || !claims.contains("entity"))
            {
                result["error"] = "Malformed token: Missing `id` or `entity` claim field.";
                return result;
            }

            result["claims"] = claims;
            result["verified"] = true;
            return result;
        }
        catch (const std::exception& e)
        {
            // Handle decoding errors (invalid token format, invalid JSON, etc.)
            result["verified"] = false;
            result["error"] = e.what();
            return result;
        }
    }
};
