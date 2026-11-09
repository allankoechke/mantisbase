#include "../../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/email.h"
#include "../../include/mantisbase/core/exceptions.h"
#include "../../include/mantisbase/utils/crypto_utils.h"
#include "../../include/mantisbase/utils/utils.h"

#include <soci/soci.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <format>
#include <limits>
#include <optional>
#include <unordered_map>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

namespace mb {
    MbAuth::MbAuth(const MantisBase &app)
        : mApp(app),
          m_oauth(std::make_unique<MbOAuthMgr>(app)),
          m_apiKeyManager(std::make_unique<MbApiKeyMgr>(app)) {
    }

    MbOAuthMgr & MbAuth::oauth() const { return *m_oauth; }

    MbApiKeyMgr & MbAuth::apiKey() const { return *m_apiKeyManager; }

    int MbAuth::sessionTimeoutSeconds(const std::string &entity_name, const int timeout) const {
        if (timeout > 0) {
            return timeout;
        }

        const auto &config = mApp.settings().configs();
        return entity_name == "mb_admins"
                   ? config.value("adminSessionTimeout", 1 * 60 * 60)
                   : config.value("sessionTimeout", 24 * 60 * 60);
    }

    std::string MbAuth::createToken(const json &claims_params, const int timeout) const {
        const auto secretKey = mApp.jwtSecretKey();
        if (claims_params.empty() || !claims_params.contains("id") || !claims_params.contains("entity")) {
            throw std::invalid_argument("Missing `id` and/or `entity` fields in token claims.");
        }

        const auto entity_name = claims_params.at("entity").get<std::string>();
        const int expiry_t = sessionTimeoutSeconds(entity_name, timeout);
        const auto &config = mApp.settings().configs();

        // Create a session record
        auto session_id = generateTimeBasedId();
        auto now = getCurrentTimestampUTC();

        const auto time = jwt::date::clock::now();
        auto token_builder = jwt::create()
                .set_type("JWT")
                .set_issued_at(time)
                .set_not_before(time)
                .set_expires_at(time + std::chrono::seconds(expiry_t))
                .set_issuer("mantisbase");

        if (config.value("jwtEnableSetIssuer", false)) {
            token_builder.set_issuer(config.at("orgName").get<std::string>());
        }
        if (config.value("jwtEnableSetAudience", false)) {
            token_builder.set_audience(config.at("siteDomain").get<std::string>());
        }

        // Add session_id to claims
        token_builder.set_payload_claim("session_id", jwt::claim(session_id));

        for (const auto &[key, value]: claims_params.items()) {
            token_builder.set_payload_claim(key, value);
        }

        const std::string token = token_builder.sign(jwt::algorithm::hs256{secretKey});

        // Store session in database
        try {
            const auto &sql = mApp.db().session();
            auto token_hash = sha256Hex(token);
            auto entity_name = claims_params.at("entity").get<std::string>();
            auto user_id = claims_params.at("id").get<std::string>();

            // Compute expiry timestamp
            auto expiry_time = std::chrono::system_clock::now() + std::chrono::seconds(expiry_t);
            auto expiry_tt = std::chrono::system_clock::to_time_t(expiry_time);
            auto *utc = std::gmtime(&expiry_tt);
            char expires_buf[20];
            std::strftime(expires_buf, sizeof(expires_buf), "%Y-%m-%d %H:%M:%S", utc);
            std::string expires_at(expires_buf);

            *sql << "INSERT INTO mb_sessions (id, entity_name, user_id, token_hash, expires_at, created) "
                    "VALUES (:id, :entity, :uid, :hash, :exp, :now)",
                    soci::use(session_id), soci::use(entity_name), soci::use(user_id),
                    soci::use(token_hash), soci::use(expires_at), soci::use(now);
        } catch (const std::exception &e) {
            // LogOrigin::authWarn("Session Creation Failed", fmt::format("Failed to create session: {}", e.what()));
        }

        return token;
    }

    json MbAuth::verifyToken(const std::string &token) const {
        json result;
        result["claims"] = json::object();
        result["error"] = "";
        result["verified"] = false;

        try {
            const auto decoded = jwt::decode(token);
            const auto secretKey = mApp.jwtSecretKey();

            auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{secretKey})
                    .with_issuer("mantisbase");

            const auto &config = mApp.settings().configs();
            if (config.value("jwtEnableSetIssuer", false)) {
                verifier.with_issuer(config.at("orgName").get<std::string>());
            }
            if (config.value("jwtEnableSetAudience", false)) {
                verifier.with_audience(config.at("siteDomain").get<std::string>());
            }

            std::error_code ec;
            verifier.verify(decoded, ec);

            if (ec) {
                // LogOrigin::authTrace("Token Verification Failed", fmt::format("Token verification failed: {}", ec.message()));
                result["error"] = ec.message();
                return result;
            }

            json claims;
            for (auto &[key, value]: decoded.get_payload_json()) {
                claims[key] = value;
            }

            if (!claims.contains("id") || !claims.contains("entity")) {
                result["error"] = "Malformed token: Missing `id` or `entity` claim field.";
                return result;
            }

            // Verify session exists in database
            if (claims.contains("session_id")) {
                try {
                    const auto &sql = mApp.db().session();
                    auto session_id = claims["session_id"].get<std::string>();
                    auto now = getCurrentTimestampUTC();

                    int count = 0;
                    *sql << "SELECT COUNT(*) FROM mb_sessions WHERE id = :id AND expires_at > :now",
                            soci::use(session_id), soci::use(now), soci::into(count);

                    if (count == 0) {
                        result["error"] = "Session expired or revoked";
                        return result;
                    }
                } catch (const std::exception &e) {
                    // LogOrigin::authWarn("Session Validation Error", fmt::format("Failed to validate session: {}", e.what()));
                }
            }

            result["claims"] = claims;
            result["verified"] = true;
            return result;
        } catch (const std::exception &) {
            result["verified"] = false;
            result["error"] = "Token verification failed.";
            return result;
        }
    }

    bool MbAuth::deleteSession(const std::string &session_id) const {
        try {
            const auto &sql = mApp.db().session();
            *sql << "DELETE FROM mb_sessions WHERE id = :id", soci::use(session_id);
            int affected = 0;
            *sql << "SELECT changes()", soci::into(affected);
            return affected > 0;
        } catch (...) {
            return false;
        }
    }

    json MbAuth::refreshSession(const std::string &old_session_id, const std::string &entity_name,
                              const std::string &user_id) {
        const auto &sql = mApp.db().session();
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

    namespace {
        constexpr int kLoginOtpTtlSeconds = 10 * 60;
        constexpr int kLoginOtpMaxAttempts = 5;

        struct OtpAccount {
            std::string id;
            std::string email;
        };

        std::string normalizeEmail(const std::string &raw) {
            auto email = trim(raw);
            toLowerCase(email);
            return email;
        }

        bool looksLikeEmail(const std::string &email) {
            if (email.empty() || email.front() == '@' || email.back() == '@')
                return false;
            const auto at = email.find('@');
            if (at == std::string::npos || email.find('@', at + 1) != std::string::npos)
                return false;
            return email.find_first_of(" \t\r\n") == std::string::npos;
        }

        bool isSixDigitCode(const std::string &otp) {
            return otp.size() == 6 &&
                   std::all_of(otp.begin(), otp.end(), [](const unsigned char c) { return std::isdigit(c) != 0; });
        }

        std::string timestampAfterSeconds(const int seconds) {
            const auto when = std::chrono::system_clock::now() + std::chrono::seconds(seconds);
            const auto tt = std::chrono::system_clock::to_time_t(when);
            const std::tm utc = toUtcTime(tt);
            char buffer[20];
            if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &utc) == 0)
                return getCurrentTimestampUTC();
            return buffer;
        }

        std::string generateLoginCode() {
            constexpr std::uint32_t span = 1000000;
            constexpr std::uint32_t limit =
                    (std::numeric_limits<std::uint32_t>::max() / span) * span;
            for (;;) {
                const auto hex = generateSecureRandom(4);
                const auto value = static_cast<std::uint32_t>(std::stoul(hex, nullptr, 16));
                if (value < limit)
                    return std::format("{:06}", value % span);
            }
        }

        const std::string &otpTimingHash() {
            static const std::string hash = hashPassword("mantisbase-otp-timing");
            return hash;
        }

        void burnOtpTiming(const std::string &otp) {
            try {
                (void) verifyPassword(otp, otpTimingHash());
            } catch (const std::exception &) {
            }
        }

        std::string appDisplayName(const MantisBase &app) {
            const auto &config = app.settings().configs();
            std::string name = "MantisBase";
            if (config.contains("orgName") && config["orgName"].is_string())
                name = trim(config["orgName"].get<std::string>());
            if (name.empty())
                name = "MantisBase";

            std::string clean;
            clean.reserve(name.size());
            for (const unsigned char c : name) {
                if (c == '\r' || c == '\n' || c == '\0')
                    clean.push_back(' ');
                else
                    clean.push_back(static_cast<char>(c));
            }
            return clean;
        }

        void purgeExpiredOtps(soci::session &sql) {
            const auto now = getCurrentTimestampUTC();
            sql << "DELETE FROM mb_login_otps WHERE expires_at <= :now", soci::use(now);
        }

        std::optional<OtpAccount> findAccountByEmail(const MantisBase &app,
                                                      const std::string &entity_name,
                                                      const std::string &email) {
            const auto entity = app.entity(entity_name);
            if (!entity.hasField("email").has_value())
                throw MbException(400, "This account cannot sign in with email.");

            const auto sql = app.db().session();
            soci::row row;
            const auto query = std::format(
                "SELECT id, email FROM {} WHERE email IS NOT NULL AND LOWER(email) = LOWER(:email) LIMIT 1",
                sqlIdentifier(entity.name()));
            *sql << query, soci::use(email), soci::into(row);
            if (!sql->got_data())
                return std::nullopt;

            OtpAccount account;
            account.id = row.get<std::string>(0);
            account.email = trim(row.get<std::string>(1));
            if (account.id.empty() || account.email.empty())
                return std::nullopt;
            return account;
        }

        [[noreturn]] void rejectLoginCode(const MantisBase &app, const std::string &entity_name) {
            app.logger().warn("MbAuth", "Sign-in code rejected", entity_name);
            throw MbException(401, "Invalid or expired sign-in code.");
        }
    } // namespace

    json MbAuth::requestLoginOtp(const std::string &entity_name, const std::string &email) const {
        if (!mApp.mailer().isConfigured())
            throw MbException(400, std::string(MbMailer::kNotConfiguredMessage));

        const auto address = normalizeEmail(email);
        if (!looksLikeEmail(address))
            throw MbException(400, "Invalid email address");

        const auto account = findAccountByEmail(mApp, entity_name, address);
        if (!account.has_value())
            return {{"sent", true}};

        auto email_key = account->email;
        toLowerCase(email_key);

        const auto id = generateTimeBasedId();
        const auto code = generateLoginCode();
        const auto code_hash = hashPassword(code);
        const auto expires_at = timestampAfterSeconds(kLoginOtpTtlSeconds);
        const auto now = getCurrentTimestampUTC();
        const auto minutes = std::to_string(kLoginOtpTtlSeconds / 60);
        const auto app_name = appDisplayName(mApp);

        const auto sql = mApp.db().session();
        purgeExpiredOtps(*sql);
        {
            soci::transaction tr(*sql);
            *sql << "INSERT INTO mb_login_otps "
                    "(id, entity_name, user_id, email, code_hash, expires_at, attempts, created) "
                    "VALUES (:id, :entity, :uid, :email, :hash, :exp, 0, :now)",
                    soci::use(id), soci::use(entity_name), soci::use(account->id),
                    soci::use(email_key), soci::use(code_hash), soci::use(expires_at), soci::use(now);
            tr.commit();
        }

        MbEmailMsg message;
        message.to = account->email;
        message.subject = std::format("Your {} sign-in code", app_name);
        const std::unordered_map<std::string, std::string> vars = {
            {"app_name", app_name},
            {"otp", code},
            {"expires_minutes", minutes}
        };
        message.body_text = mApp.mailer().loadTemplate("login_otp.txt", vars);
        message.body_html = mApp.mailer().loadTemplate("login_otp.html", vars);

        try {
            if (!mApp.mailer().send(message))
                throw MbException(502, "Failed to send the sign-in code.");
        } catch (...) {
            try {
                *sql << "DELETE FROM mb_login_otps WHERE id = :id", soci::use(id);
            } catch (const std::exception &) {
            }
            throw;
        }

        // A failed send leaves any previous code in place. After a successful send,
        // only the code that was just delivered remains valid.
        *sql << "DELETE FROM mb_login_otps WHERE entity_name = :entity AND user_id = :uid AND id != :id",
                soci::use(entity_name), soci::use(account->id), soci::use(id);

        return {{"sent", true}};
    }

    json MbAuth::loginWithOtp(const std::string &entity_name,
                              const std::string &email,
                              const std::string &otp) const {
        const auto address = normalizeEmail(email);
        if (!looksLikeEmail(address))
            throw MbException(400, "Invalid email address");

        const auto code = trim(otp);
        if (!isSixDigitCode(code))
            throw MbException(400, "Sign-in code must be a 6-digit number.");

        const auto sql = mApp.db().session();
        purgeExpiredOtps(*sql);

        std::string id;
        std::string user_id;
        std::string code_hash;
        int attempts = 0;
        const auto now = getCurrentTimestampUTC();
        *sql << "SELECT id, user_id, code_hash, attempts FROM mb_login_otps "
                "WHERE entity_name = :entity AND email = :email AND expires_at > :now "
                "ORDER BY created DESC, id DESC LIMIT 1",
                soci::use(entity_name), soci::use(address), soci::use(now),
                soci::into(id), soci::into(user_id), soci::into(code_hash), soci::into(attempts);

        if (!sql->got_data() || id.empty() || code_hash.empty()) {
            burnOtpTiming(code);
            rejectLoginCode(mApp, entity_name);
        }

        if (attempts >= kLoginOtpMaxAttempts) {
            *sql << "DELETE FROM mb_login_otps WHERE id = :id", soci::use(id);
            burnOtpTiming(code);
            rejectLoginCode(mApp, entity_name);
        }

        *sql << "UPDATE mb_login_otps SET attempts = attempts + 1 WHERE id = :id", soci::use(id);

        bool matched = false;
        try {
            matched = verifyPassword(code, code_hash);
        } catch (const std::exception &) {
            matched = false;
        }

        if (!matched) {
            if (attempts + 1 >= kLoginOtpMaxAttempts)
                *sql << "DELETE FROM mb_login_otps WHERE id = :id", soci::use(id);
            rejectLoginCode(mApp, entity_name);
        }

        *sql << "DELETE FROM mb_login_otps WHERE entity_name = :entity AND user_id = :uid",
                soci::use(entity_name), soci::use(user_id);

        const auto entity = mApp.entity(entity_name);
        auto user_opt = entity.read(user_id);
        if (!user_opt.has_value())
            rejectLoginCode(mApp, entity_name);

        auto user = std::move(user_opt.value());
        user.erase("password");
        const auto token = createToken({{"id", user_id}, {"entity", entity_name}});
        return {
            {"token", token},
            {"user", user}
        };
    }

#ifdef MB_SCRIPTING_ENABLED
    std::string MbAuth::createTokenJson(const std::string &claims_json, const int timeout) const {
        return createToken(json::parse(claims_json), timeout);
    }

    std::string MbAuth::verifyTokenJson(const std::string &token) const {
        return verifyToken(token).dump();
    }

    std::string MbAuth::refreshSessionJson(const std::string &old_session_id,
                                         const std::string &entity_name,
                                         const std::string &user_id) {
        return refreshSession(old_session_id, entity_name, user_id).dump();
    }
#endif
};
