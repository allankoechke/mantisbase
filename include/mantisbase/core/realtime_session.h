#ifndef MANTISBASE_REALTIME_SESSION_H
#define MANTISBASE_REALTIME_SESSION_H

#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <drogon/HttpRequest.h>
#include <nlohmann/json.hpp>

namespace mb {
    class MantisBase;

    using json = nlohmann::json;

    inline constexpr int kRealtimeNoTopicGraceSeconds = 60;
    inline constexpr int kRealtimeIdleTimeoutMinutes = 10;
    inline constexpr int kRealtimeCleanupIntervalSeconds = 30;

    struct RealtimeAuthSnapshot {
        json auth = json::object();
        json verification = json::object();
        std::string token;
        std::chrono::system_clock::time_point expires_at{};
    };

    struct TopicAccessResult {
        std::vector<std::string> granted;
        std::vector<json> denied;
    };

    enum class AuthUpgradeResult {
        Applied,
        Unchanged,
        DeniedSwitch,
        DeniedDowngrade
    };

    struct RealtimeWsSession {
        std::string client_id;
        RealtimeAuthSnapshot auth;
        std::set<std::string> topics;
        std::chrono::steady_clock::time_point connected_at = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();
    };

    [[nodiscard]] RealtimeAuthSnapshot makeGuestAuthSnapshot();

    [[nodiscard]] std::string resolveRealtimeToken(const drogon::HttpRequestPtr &req);

    [[nodiscard]] RealtimeAuthSnapshot resolveRealtimeAuth(const MantisBase &app, const std::string &token);

    [[nodiscard]] TopicAccessResult filterAuthorizedTopics(const MantisBase &app,
                                                             const std::vector<std::string> &topics,
                                                             const RealtimeAuthSnapshot &snap);

    [[nodiscard]] AuthUpgradeResult tryUpgradeAuth(RealtimeAuthSnapshot &session,
                                                   const RealtimeAuthSnapshot &incoming);

    [[nodiscard]] std::string generateRealtimeClientId(std::string_view prefix);

    [[nodiscard]] bool isAuthExpired(const RealtimeAuthSnapshot &snap, const MantisBase &app);

    [[nodiscard]] bool isAuthenticatedSnapshot(const RealtimeAuthSnapshot &snap);

    void touchWsSession(RealtimeWsSession &session);

} // namespace mb

#endif // MANTISBASE_REALTIME_SESSION_H
