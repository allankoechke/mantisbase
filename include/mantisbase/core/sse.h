/**
 * @file sse.h
 * @brief Server-Sent Events (SSE) manager for the realtime API.
 *
 * Exposes GET and POST /api/v1/realtime:
 * - **GET /api/v1/realtime?topics=...** — Opens an SSE connection with a comma-separated
 *   list of topics (entity names or entity:row_id). Returns a session (client_id) and
 *   streams events: connected, ping, change (insert/update/delete).
 * - **POST /api/v1/realtime** — Updates topics for an existing session (JSON body:
 *   client_id, topics). Clearing topics disconnects the SSE session.
 *
 * Change events are driven by realtime.h (SQLite/PostgreSQL). Access control uses
 * entity list/get rules for the requested topics.
 * @see realtime.h
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

namespace mb {
    using json = nlohmann::json;
    class WSMgr;

    /** Per-client SSE session: holds subscribed topics and a Drogon async stream for zero-thread event delivery. */
    class SSESession {
        std::string m_clientID;
        std::set<std::string> m_topics;
        drogon::ResponseStreamPtr m_stream;
        mutable std::mutex m_topicsMutex;

        std::atomic<bool> m_isActive;
        std::chrono::steady_clock::time_point m_lastActivity;

    public:
        SSESession(std::string client_id,
                   const std::set<std::string> &topics,
                   drogon::ResponseStreamPtr stream);

        /** Send an SSE-formatted event through the async stream. Returns false if stream is closed. */
        bool sendEvent(const std::string &eventType, const json &data);

        /** True if this session is subscribed to the topic implied by change_event. */
        bool isInterestedIn(const json &change_event) const;

        /** Format a change event for SSE (topic, action, entity, row_id, data, timestamp). */
        json formatEvent(const json &change_event) const;

        void updateActivity();

        void updateTopics(const std::set<std::string> &topics);

        std::chrono::steady_clock::time_point getLastActivity() const;

        /** Mark session inactive and close the stream. */
        void close();

        bool isActive() const;

        const std::string &getClientID() const;

        std::set<std::string> getTopics() const;

        void setTopics(const std::set<std::string> &topics);
    };

    /** Manages SSE sessions, WebSocket connections, routes realtime change events, and registers GET/POST /api/v1/realtime. */
    class SSEMgr {
        std::unordered_map<std::string, std::shared_ptr<SSESession>> m_sessions;
        std::mutex m_sessions_mutex;
        std::condition_variable m_cv;
        std::thread m_cleanup_thread;
        std::atomic<bool> m_running{true};
        std::unique_ptr<WSMgr> m_wsMgr;

    public:
        SSEMgr();
        ~SSEMgr();

        /** Register GET and POST /api/v1/realtime routes. */
        void createRoutes();
        /** Create a new SSE session with the given topics and stream; returns client_id. */
        std::string createSession(const std::set<std::string> &initial_topics,
                                  drogon::ResponseStreamPtr stream);

        std::shared_ptr<SSESession> fetchSession(const std::string &session_id);
        /** Remove session and close it (disconnect). */
        void removeSession(const std::string &session_id);

        void updateActivity(const std::string &session_id);
        std::shared_ptr<mb::SSESession> getSession(const std::string &sessionId);

        /** Push a change event to all SSE sessions and WS connections interested in its topic. */
        void broadcastChange(const json &change_event);
        size_t getSessionCount();

        WSMgr &wsMgr() const;

        void start();
        void stop();
        bool isRunning() const;

    private:
        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> validateSubTopics(bool is_updating = false);

        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> validateHasAccess();

        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> updateAuthTokenForSSE();

        static std::string generateClientID();

        void cleanupIdleSessions();
    };
}


#endif //MANTISBASE_SSE_H
