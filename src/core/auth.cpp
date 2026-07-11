#include "../../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/utils/crypto_utils.h"

#include <cstring>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

namespace mb
{
    std::string Auth::createToken(const json& claims_params, const int timeout)
    {
        const auto secretKey = MantisBase::jwtSecretKey();
        if (claims_params.empty() || !claims_params.contains("id") || !claims_params.contains("entity"))
        {
            throw std::invalid_argument("Missing `id` and/or `entity` fields in token claims.");
        }

        const auto& config = json::object();
        const int expiry_t = timeout > 0
                                 ? timeout
                                 : claims_params.at("entity").get<std::string>() == "mb_admins"
                                 ? config.value("adminSessionTimeout", 1 * 60 * 60)
                                 : config.value("sessionTimeout", 24 * 60 * 60);

        // Create a session record
        auto session_id = generateTimeBasedId();
        auto now = getCurrentTimestampUTC();

        const auto time = jwt::date::clock::now();
        auto token_builder = jwt::create()
                             .set_type("JWT")
                             .set_issued_at(time)
                             .set_not_before(time)
                             .set_expires_at(time + std::chrono::seconds(expiry_t));

        if (config.value("jwtEnableSetIssuer", false))
        {
            token_builder.set_issuer(config.at("appName").get<std::string>());
        }
        if (config.value("jwtEnableSetAudience", false))
        {
            token_builder.set_audience(config.at("baseUrl").get<std::string>());
        }

        // Add session_id to claims
        token_builder.set_payload_claim("session_id", jwt::claim(session_id));

        for (const auto& [key, value] : claims_params.items())
        {
            token_builder.set_payload_claim(key, value);
        }

        const std::string token = token_builder.sign(jwt::algorithm::hs256{secretKey});

        // Store session in database
        try {
            auto sql = MantisBase::instance().db().session();
            auto token_hash = sha256Hex(token);
            auto entity_name = claims_params.at("entity").get<std::string>();
            auto user_id = claims_params.at("id").get<std::string>();

            // Compute expiry timestamp
            auto expiry_time = std::chrono::system_clock::now() + std::chrono::seconds(expiry_t);
            auto expiry_tt = std::chrono::system_clock::to_time_t(expiry_time);
            auto* utc = std::gmtime(&expiry_tt);
            char expires_buf[20];
            std::strftime(expires_buf, sizeof(expires_buf), "%Y-%m-%d %H:%M:%S", utc);
            std::string expires_at(expires_buf);

            *sql << "INSERT INTO mb_sessions (id, entity_name, user_id, token_hash, expires_at, created) "
                    "VALUES (:id, :entity, :uid, :hash, :exp, :now)",
                soci::use(session_id), soci::use(entity_name), soci::use(user_id),
                soci::use(token_hash), soci::use(expires_at), soci::use(now);
        } catch (const std::exception& e) {
            LogOrigin::authWarn("Session Creation Failed", fmt::format("Failed to create session: {}", e.what()));
        }

        return token;
    }

    json Auth::verifyToken(const std::string& token)
    {
        json result;
        result["claims"] = json::object();
        result["error"] = "";
        result["verified"] = false;

        try
        {
            const auto decoded = jwt::decode(token);
            const auto secretKey = MantisBase::jwtSecretKey();

            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secretKey});

            const auto& config = json::object();
            if (config.value("jwtEnableSetIssuer", false))
            {
                verifier.with_issuer(config.at("appName").get<std::string>());
            }
            if (config.value("jwtEnableSetAudience", false))
            {
                verifier.with_audience(config.at("baseUrl").get<std::string>());
            }

            std::error_code ec;
            verifier.verify(decoded, ec);

            if (ec)
            {
                LogOrigin::authTrace("Token Verification Failed", fmt::format("Token verification failed: {}", ec.message()));
                result["error"] = ec.message();
                return result;
            }

            json claims;
            for (auto& [key, value] : decoded.get_payload_json())
            {
                claims[key] = value;
            }

            if (!claims.contains("id") || !claims.contains("entity"))
            {
                result["error"] = "Malformed token: Missing `id` or `entity` claim field.";
                return result;
            }

            // Verify session exists in database
            if (claims.contains("session_id")) {
                try {
                    auto sql = MantisBase::instance().db().session();
                    auto session_id = claims["session_id"].get<std::string>();
                    auto now = getCurrentTimestampUTC();

                    int count = 0;
                    *sql << "SELECT COUNT(*) FROM mb_sessions WHERE id = :id AND expires_at > :now",
                        soci::use(session_id), soci::use(now), soci::into(count);

                    if (count == 0) {
                        result["error"] = "Session expired or revoked";
                        return result;
                    }
                } catch (const std::exception& e) {
                    LogOrigin::authWarn("Session Validation Error", fmt::format("Failed to validate session: {}", e.what()));
                }
            }

            result["claims"] = claims;
            result["verified"] = true;
            return result;
        }
        catch (const std::exception& e)
        {
            result["verified"] = false;
            result["error"] = e.what();
            return result;
        }
    }

    bool Auth::deleteSession(const std::string& session_id) {
        try {
            auto sql = MantisBase::instance().db().session();
            *sql << "DELETE FROM mb_sessions WHERE id = :id", soci::use(session_id);
            int affected = 0;
            *sql << "SELECT changes()", soci::into(affected);
            return affected > 0;
        } catch (...) {
            return false;
        }
    }

    json Auth::refreshSession(const std::string& old_session_id, const std::string& entity_name,
                              const std::string& user_id) {
        auto sql = MantisBase::instance().db().session();
        auto now = getCurrentTimestampUTC();

        // Verify old session exists
        int count = 0;
        *sql << "SELECT COUNT(*) FROM mb_sessions WHERE id = :id AND entity_name = :entity AND user_id = :uid",
            soci::use(old_session_id), soci::use(entity_name), soci::use(user_id), soci::into(count);

        if (count == 0) {
            throw std::runtime_error("Session not found or does not belong to this user");
        }

        // Delete old session
        *sql << "DELETE FROM mb_sessions WHERE id = :id", soci::use(old_session_id);

        // Create new token (which creates a new session)
        auto token = createToken({{"id", user_id}, {"entity", entity_name}});

        return {
            {"token", token}
        };
    }
};
