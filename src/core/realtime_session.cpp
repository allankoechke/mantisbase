#include "../../include/mantisbase/core/realtime_session.h"

#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/api_keys.h"
#include "../../include/mantisbase/core/models/access_rules.h"
#include "../../include/mantisbase/utils/utils.h"

#include <atomic>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

namespace mb {

    namespace {

        std::string trimToken(std::string value) {
            return trim(std::move(value));
        }

        std::pair<std::string, std::string> parseTopicParts(const std::string &topic) {
            auto entity_name = topic;
            std::string record_id;
            if (auto pos = topic.find(':'); pos != std::string::npos) {
                entity_name = topic.substr(0, pos);
                record_id = topic.substr(pos + 1);
                if (record_id == "*") {
                    record_id.clear();
                }
            }
            return {entity_name, record_id};
        }

        std::optional<std::chrono::system_clock::time_point> jwtExpiresAt(const std::string &token) {
            try {
                const auto decoded = jwt::decode(token);
                if (!decoded.has_expires_at()) {
                    return std::nullopt;
                }
                const auto exp = decoded.get_expires_at();
                return std::chrono::system_clock::time_point{
                    std::chrono::duration_cast<std::chrono::system_clock::duration>(exp.time_since_epoch())};
            } catch (...) {
                return std::nullopt;
            }
        }

        void hydrateUserRecord(const MantisBase &app, json &auth) {
            if (!auth.contains("entity") || !auth["entity"].is_string() ||
                !auth.contains("id") || auth["id"].is_null()) {
                return;
            }

            try {
                const auto entity_name = auth["entity"].get<std::string>();
                const auto user_id = auth["id"].get<std::string>();
                const auto user_entity = app.entity(entity_name);
                if (auto user = user_entity.read(user_id); user.has_value()) {
                    auto u = user.value();
                    u.erase("password");
                    auth["user"] = u;
                }
            } catch (...) {
            }
        }

        std::pair<std::string, std::string> authIdentity(const RealtimeAuthSnapshot &snap) {
            if (!isAuthenticatedSnapshot(snap)) {
                return {"", ""};
            }

            const auto &auth = snap.auth;
            const auto entity = auth.contains("entity") && auth["entity"].is_string()
                                    ? auth["entity"].get<std::string>()
                                    : std::string{};
            std::string id;
            if (auth.contains("user") && auth["user"].is_object() && auth["user"].contains("id") &&
                auth["user"]["id"].is_string()) {
                id = auth["user"]["id"].get<std::string>();
            } else if (auth.contains("id") && auth["id"].is_string()) {
                id = auth["id"].get<std::string>();
            }
            return {entity, id};
        }
    } // namespace

    RealtimeAuthSnapshot makeGuestAuthSnapshot() {
        RealtimeAuthSnapshot snap;
        snap.auth = {
            {"type", "guest"},
            {"mode", "none"},
            {"token", nullptr},
            {"id", nullptr},
            {"entity", nullptr},
            {"user", nullptr},
            {"auth_method", nullptr}
        };
        snap.verification = json::object();
        snap.token.clear();
        snap.expires_at = {};
        return snap;
    }

    std::string resolveRealtimeToken(const drogon::HttpRequestPtr &req) {
        if (req->getParameter("token").length() > 0) {
            return trimToken(req->getParameter("token"));
        }

        const auto auth_header = req->getHeader("Authorization");
        if (auth_header.starts_with("Bearer ")) {
            return trimToken(auth_header.substr(7));
        }

        return {};
    }

    RealtimeAuthSnapshot resolveRealtimeAuth(const MantisBase &app, const std::string &token) {
        if (token.empty()) {
            return makeGuestAuthSnapshot();
        }

        RealtimeAuthSnapshot snap;
        snap.token = token;

        if (token.starts_with("mb_sk_")) {
            const auto key_hash = ApiKeyManager::hashApiKey(token);
            auto key_info = app.auth().apiKey().lookupByHash(key_hash);
            if (!key_info.has_value()) {
                return makeGuestAuthSnapshot();
            }

            const auto entity_name = key_info.value()["entity_name"].get<std::string>();
            snap.auth = makeGuestAuthSnapshot().auth;
            snap.auth["type"] = entity_name == "mb_admins" ? "admin" : "user";
            snap.auth["mode"] = "api";
            snap.auth["auth_method"] = "api_key";
            snap.auth["entity"] = entity_name;
            snap.auth["id"] = key_info.value()["user_id"];
            snap.auth["token"] = token;
            snap.verification = {
                {"verified", true},
                {"claims", {{"id", snap.auth["id"]}, {"entity", snap.auth["entity"]}}},
                {"error", ""}
            };
            hydrateUserRecord(app, snap.auth);
            snap.expires_at = {};
            return snap;
        }

        const json verification = app.auth().verifyToken(token);
        if (!verification.contains("verified") || !verification["verified"].is_boolean() ||
            !verification["verified"].get<bool>()) {
            return makeGuestAuthSnapshot();
        }

        const auto claims = verification["claims"];
        const auto entity_name = claims["entity"].get<std::string>();
        const auto user_id = claims["id"].get<std::string>();

        snap.auth = makeGuestAuthSnapshot().auth;
        snap.auth["type"] = entity_name == "mb_admins" ? "admin" : "user";
        snap.auth["mode"] = "jwt";
        snap.auth["auth_method"] = "jwt";
        snap.auth["entity"] = entity_name;
        snap.auth["id"] = user_id;
        snap.auth["token"] = token;
        snap.verification = verification;
        hydrateUserRecord(app, snap.auth);

        if (auto exp = jwtExpiresAt(token); exp.has_value()) {
            snap.expires_at = exp.value();
        }

        return snap;
    }

    TopicAccessResult filterAuthorizedTopics(const MantisBase &app,
                                             const std::vector<std::string> &topics,
                                             const RealtimeAuthSnapshot &snap) {
        TopicAccessResult result;

        for (const auto &topic : topics) {
            if (topic.empty()) {
                continue;
            }

            const auto [entity_name, record_id] = parseTopicParts(topic);

            try {
                const auto entity = app.entity(entity_name);
                const auto rule = record_id.empty() ? entity.listRule() : entity.getRule();
                AccessEvalContext ctx{snap.auth, snap.verification, nullptr};
                const auto eval = evaluateAccessRule(rule, ctx);
                if (eval == AccessEvalResult::Allow) {
                    result.granted.push_back(topic);
                    continue;
                }

                const auto [status, error] = accessEvalHttpError(eval, rule);
                std::string message = error;
                if (rule.mode().empty()) {
                    message = std::format("Admin auth required to access record(s) in `{}` entity.", entity_name);
                } else if (eval == AccessEvalResult::DenyUnknownRule) {
                    message = std::format("Access denied, entity `{}` access rule unknown.", entity_name);
                } else if (eval == AccessEvalResult::DenyUnauthenticated &&
                           snap.verification.contains("error") && snap.verification["error"].is_string() &&
                           !snap.verification["error"].get<std::string>().empty()) {
                    message = snap.verification["error"].get<std::string>();
                }

                result.denied.push_back({
                    {"topic", topic},
                    {"reason", message},
                    {"status", status}
                });
            } catch (...) {
                result.denied.push_back({
                    {"topic", topic},
                    {"reason", "Invalid topic name, expected valid entity name."},
                    {"status", 400}
                });
            }
        }

        return result;
    }

    AuthUpgradeResult tryUpgradeAuth(RealtimeAuthSnapshot &session, const RealtimeAuthSnapshot &incoming) {
        const bool session_authed = isAuthenticatedSnapshot(session);
        const bool incoming_authed = isAuthenticatedSnapshot(incoming);

        if (!session_authed && !incoming_authed) {
            return AuthUpgradeResult::Unchanged;
        }

        if (session_authed && !incoming_authed) {
            return AuthUpgradeResult::DeniedDowngrade;
        }

        if (!session_authed && incoming_authed) {
            session = incoming;
            return AuthUpgradeResult::Applied;
        }

        const auto [session_entity, session_id] = authIdentity(session);
        const auto [incoming_entity, incoming_id] = authIdentity(incoming);

        if (session_entity != incoming_entity || session_id != incoming_id) {
            return AuthUpgradeResult::DeniedSwitch;
        }

        session = incoming;
        return AuthUpgradeResult::Applied;
    }

    std::string generateRealtimeClientId(const std::string_view prefix) {
        static std::atomic<uint64_t> counter{0};
        const auto now = std::chrono::system_clock::now().time_since_epoch().count();
        return std::format("{}_{}_{}{}", prefix, now, counter++, generateShortId(5));
    }

    bool isAuthenticatedSnapshot(const RealtimeAuthSnapshot &snap) {
        if (isGuestAuth(snap.auth)) {
            return false;
        }

        return snap.verification.contains("verified") && snap.verification["verified"].is_boolean() &&
               snap.verification["verified"].get<bool>() && snap.auth.contains("user") &&
               snap.auth["user"].is_object() && !snap.auth["user"].is_null();
    }

    bool isAuthExpired(const RealtimeAuthSnapshot &snap, const MantisBase &app) {
        if (!isAuthenticatedSnapshot(snap) || snap.token.empty()) {
            return false;
        }

        if (snap.token.starts_with("mb_sk_")) {
            const auto key_hash = ApiKeyManager::hashApiKey(snap.token);
            return !app.auth().apiKey().lookupByHash(key_hash).has_value();
        }

        const json verification = app.auth().verifyToken(snap.token);
        if (!verification.contains("verified") || !verification["verified"].is_boolean() ||
            !verification["verified"].get<bool>()) {
            return true;
        }

        if (snap.expires_at != std::chrono::system_clock::time_point{}) {
            return std::chrono::system_clock::now() >= snap.expires_at;
        }

        return false;
    }

    void touchWsSession(RealtimeWsSession &session) {
        session.last_activity = std::chrono::steady_clock::now();
    }

} // namespace mb
