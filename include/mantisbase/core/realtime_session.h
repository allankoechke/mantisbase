/**
 * @file realtime_session.h
 * @brief Shared auth, topic filtering, and session helpers for SSE and WebSocket realtime.
 *
 * Implements the PocketBase-style two-phase realtime model:
 * - **Connect**: optional credentials; invalid tokens become guest.
 * - **Subscribe**: re-resolve token, upgrade auth, filter topics by entity list/get rules.
 *
 * Token precedence (realtime only): `?token=` query param overrides `Authorization: Bearer`.
 * Session IDs use the prefixes `rt_sse_` and `rt_ws_`.
 *
 * @see sse.h
 * @see ws.h
 * @see access_rules.h
 */

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

    /** Grace period before closing a connect-only session with no subscribed topics. */
    inline constexpr int kRealtimeNoTopicGraceSeconds = 60;

    /** Idle timeout for sessions that have active topic subscriptions. */
    inline constexpr int kRealtimeIdleTimeoutMinutes = 10;

    /** Background cleanup loop interval for SSE and WebSocket stale sessions. */
    inline constexpr int kRealtimeCleanupIntervalSeconds = 30;

    /**
     * @brief Auth state captured at connect or subscribe time.
     *
     * `auth` and `verification` mirror the shape produced by HTTP auth middleware.
     * `token` is empty for guest sessions. `expires_at` is set for JWT-backed sessions.
     */
    struct RealtimeAuthSnapshot {
        json auth = json::object();
        json verification = json::object();
        std::string token;
        std::chrono::system_clock::time_point expires_at{};
    };

    /** Result of filtering a subscribe request against entity access rules. */
    struct TopicAccessResult {
        std::vector<std::string> granted;
        /** Each denied entry: `{ "topic", "reason", "status" }`. */
        std::vector<json> denied;
    };

    /** Outcome of applying incoming credentials to an existing realtime session. */
    enum class AuthUpgradeResult {
        Applied,        /**< Guest upgraded to authenticated, or same-user token refreshed. */
        Unchanged,      /**< Both session and incoming are guest, or same authenticated user. */
        DeniedSwitch,   /**< Attempt to switch to a different authenticated user. */
        DeniedDowngrade /**< Attempt to downgrade an authenticated session to guest. */
    };

    /** Per-connection WebSocket session state (also stored on the Drogon connection context). */
    struct RealtimeWsSession {
        std::string client_id;
        RealtimeAuthSnapshot auth;
        std::set<std::string> topics;
        std::chrono::steady_clock::time_point connected_at = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();
    };

    /** @return Guest auth snapshot (`type: guest`) with empty token and verification. */
    [[nodiscard]] RealtimeAuthSnapshot makeGuestAuthSnapshot();

    /**
     * @brief Resolve bearer/API-key token from a realtime HTTP upgrade or SSE request.
     * @param req Drogon request (`?token=` wins over `Authorization: Bearer`).
     * @return Trimmed token string, or empty for unauthenticated connect.
     */
    [[nodiscard]] std::string resolveRealtimeToken(const drogon::HttpRequestPtr &req);

    /**
     * @brief Hydrate auth from a JWT or `mb_sk_` API key.
     * @param app Application instance (for verification and user hydration).
     * @param token Raw token; empty yields guest. Invalid/expired tokens yield guest.
     */
    [[nodiscard]] RealtimeAuthSnapshot resolveRealtimeAuth(const MantisBase &app, const std::string &token);

    /**
     * @brief Filter topics by entity access rules.
     *
     * Entity-only topics use the list rule; `entity:row_id` topics use the get rule.
     *
     * @param app Application instance (entity schema lookup).
     * @param topics Requested topic strings.
     * @param snap Current session auth snapshot.
     */
    [[nodiscard]] TopicAccessResult filterAuthorizedTopics(const MantisBase &app,
                                                             const std::vector<std::string> &topics,
                                                             const RealtimeAuthSnapshot &snap);

    /**
     * @brief Apply incoming auth to an existing session (subscribe-time upgrade).
     * @param session Session snapshot to update when upgrade is allowed.
     * @param incoming Auth resolved from the subscribe request.
     */
    [[nodiscard]] AuthUpgradeResult tryUpgradeAuth(RealtimeAuthSnapshot &session,
                                                   const RealtimeAuthSnapshot &incoming);

    /**
     * @brief Generate a unique realtime client/session ID.
     * @param prefix `"rt_sse"` or `"rt_ws"`.
     */
    [[nodiscard]] std::string generateRealtimeClientId(std::string_view prefix);

    /** @return `true` when the snapshot JWT/session has expired or been revoked. */
    [[nodiscard]] bool isAuthExpired(const RealtimeAuthSnapshot &snap, const MantisBase &app);

    /** @return `true` when the snapshot represents a verified, hydrated authenticated user. */
    [[nodiscard]] bool isAuthenticatedSnapshot(const RealtimeAuthSnapshot &snap);

    /** Update `last_activity` on a WebSocket session to the current time. */
    void touchWsSession(RealtimeWsSession &session);

} // namespace mb

#endif // MANTISBASE_REALTIME_SESSION_H
