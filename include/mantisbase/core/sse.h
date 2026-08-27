/**
 * @file sse.h
 * @brief Server-Sent Events (SSE) manager for the realtime API.
 *
 * Exposes GET and POST /api/v1/realtime:
 * - **GET /api/v1/realtime** — Opens an SSE connection (optional `topics` query). Returns
 *   `client_id` in the `connected` event.
 * - **POST /api/v1/realtime** — Sets topics for an existing session (JSON body: client_id,
 *   topics, optional token). Returns granted topics and denied entries.
 *
 * Authentication is optional at connect; invalid tokens are treated as guest. Auth can be
 * upgraded on POST subscribe but not downgraded or switched to another user.
 *
 * @see realtime_session.h
 * @see realtime.h
 * @see ws.h
 */

#ifndef MANTISBASE_SSE_H
#define MANTISBASE_SSE_H

#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <drogon/HttpResponse.h>

#include "realtime.h"
#include "realtime_session.h"
#include "types.h"

namespace mb {
    using json = nlohmann::json;
    class WSMgr;

    /**
     * @brief Per-client SSE session state and async stream writer.
     *
     * Holds subscribed topics, auth snapshot, activity timestamps, and a Drogon
     * @ref drogon::ResponseStreamPtr for zero-thread event delivery.
     */
    class SSESession {
        std::string m_clientID;
        std::set<std::string> m_topics;
        drogon::ResponseStreamPtr m_stream;
        mutable std::mutex m_topicsMutex;

        std::atomic<bool> m_isActive;
        std::chrono::steady_clock::time_point m_lastActivity;
        std::chrono::steady_clock::time_point m_connectedAt;

        RealtimeAuthSnapshot m_auth;

    public:
        SSESession(std::string client_id,
                   const std::set<std::string> &topics,
                   drogon::ResponseStreamPtr stream,
                   RealtimeAuthSnapshot auth = makeGuestAuthSnapshot());

        /** Send an SSE frame (`event:` + `data:`). Returns false if the stream is closed. */
        bool sendEvent(const std::string &eventType, const json &data);

        /** @return `true` if this session should receive the given change event. */
        bool isInterestedIn(const json &change_event) const;

        /** Format a raw DB change row into the public SSE `change` event shape. */
        json formatEvent(const json &change_event) const;

        /** Bump last-activity timestamp (keepalive and idle tracking). */
        void updateActivity();

        /** Replace topics (legacy alias; prefer @ref setTopics). */
        void updateTopics(const std::set<std::string> &topics);

        [[nodiscard]] std::chrono::steady_clock::time_point getLastActivity() const;
        [[nodiscard]] std::chrono::steady_clock::time_point getConnectedAt() const;

        /** Close the stream and mark the session inactive. */
        void close();

        [[nodiscard]] bool isActive() const;

        [[nodiscard]] const std::string &getClientID() const;

        [[nodiscard]] std::set<std::string> getTopics() const;

        /** Replace the full topic set (subscribe replaces prior subscriptions). */
        void setTopics(const std::set<std::string> &topics);

        [[nodiscard]] RealtimeAuthSnapshot &authSnapshot();
        [[nodiscard]] const RealtimeAuthSnapshot &authSnapshot() const;

        /** Update auth after a successful subscribe-time upgrade. */
        void setAuthSnapshot(RealtimeAuthSnapshot auth);
    };

    /**
     * @brief Manages SSE routes, sessions, broadcast, and shared WebSocket cleanup.
     *
     * Owns a @ref WSMgr instance and runs a background thread that closes expired,
     * idle, and connect-only sessions for both SSE and WebSocket.
     */
    class SSEMgr : public IMantisBase {
        std::unordered_map<std::string, std::shared_ptr<SSESession>> m_sessions;
        std::mutex m_sessions_mutex;
        std::condition_variable m_cv;
        std::thread m_cleanup_thread;
        std::atomic<bool> m_running{true};
        std::unique_ptr<WSMgr> m_wsMgr;

    public:
        explicit SSEMgr(const MantisBase& app);
        ~SSEMgr();

        /** Register GET/POST `/api/v1/realtime` and the WebSocket controller. */
        void createRoutes();

        /**
         * @brief Create a session and attach it to an async response stream.
         * @return Client ID (`rt_sse_...`), or empty on failure.
         */
        std::string createSession(const std::set<std::string> &initial_topics,
                                  drogon::ResponseStreamPtr stream,
                                  RealtimeAuthSnapshot auth = makeGuestAuthSnapshot());

        /** Look up a session by ID without updating activity. */
        std::shared_ptr<SSESession> fetchSession(const std::string &session_id);

        /** Remove and close a session by client ID. */
        void removeSession(const std::string &session_id);

        /** Bump last-activity for an existing session. */
        void updateActivity(const std::string &session_id);

        /** Look up a session by client ID (used by POST subscribe handler). */
        std::shared_ptr<mb::SSESession> getSession(const std::string &sessionId);

        /** Push a change event to all SSE sessions subscribed to the matching topic. */
        void broadcastChange(const json &change_event);

        /** @return Number of active SSE sessions. */
        size_t getSessionCount();

        /** @return WebSocket manager (shared cleanup loop). */
        WSMgr &wsMgr() const;

        /** Start the cleanup background thread. */
        void start();

        /** Stop cleanup and the realtime worker. */
        void stop();

        [[nodiscard]] bool isRunning() const;

    private:
        static std::function<void(MantisRequest &, MantisResponse &)> handleSSESessionUpdate();

        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> validateSubTopics(bool is_updating = false);

        void cleanupIdleSessions();
    };
}


#endif //MANTISBASE_SSE_H
