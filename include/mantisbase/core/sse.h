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

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "realtime.h"

namespace mb {
    using json = nlohmann::json;

    /** Per-client SSE session: holds subscribed topics and queues events (change, ping). */
    class SSESession {
        std::string m_clientID;
        std::set<std::string> m_topics;
        std::string authToken;

        std::mutex m_queueMutex;
        std::condition_variable m_queueCV;
        std::queue<std::pair<std::string, json> > m_eventQueue; // <event_type, data>

        std::atomic<bool> m_isActive;
        std::chrono::steady_clock::time_point m_lastActivity;

    public:
        SSESession(const std::string &sessionId,
                   const std::set<std::string> &topics);

        // Queue an event to be sent
        /** Queue an event (e.g. "change", "ping") to be sent to the client. */
        void queueEvent(const std::string &eventType, const json &data);

        /** Block until the next event is available or timeout; returns true if event was read. */
        bool waitForEvent(std::string &eventType, json &data,
                          std::chrono::milliseconds timeout);

        /** True if this session is subscribed to the topic implied by change_event. */
        bool isInterestedIn(const json &change_event) const;

        /** Format a change event for SSE (topic, action, entity, row_id, data, timestamp). */
        json formatEvent(const json &change_event) const;

        void updateActivity();

        void updateTopics(std::set<std::string> &topics);

        auto getLastActivity() const;

        /** Mark session inactive and wake any waiters (disconnect). */
        void close();

        bool isActive() const;

        const std::string &getClientID() const;

        const std::set<std::string> &getTopics() const;

        void setTopics(const std::set<std::string> &topics);
    };

    /** Manages SSE sessions, routes realtime change events, and registers GET/POST /api/v1/realtime. */
    class SSEMgr {
        std::unordered_map<std::string, std::shared_ptr<SSESession>> m_sessions;
        std::mutex m_sessions_mutex;
        std::condition_variable m_cv;
        std::thread m_cleanup_thread;
        std::atomic<bool> m_running{true};

    public:
        SSEMgr() = default;
        ~SSEMgr();

        /** Register GET and POST /api/v1/realtime routes. */
        static void createRoutes();
        /** Create a new SSE session with the given topics; returns client_id (session id). */
        std::string createSession(const std::set<std::string> &initial_topics);

        std::shared_ptr<SSESession> fetchSession(const std::string &session_id);
        /** Remove session and close it (disconnect). */
        void removeSession(const std::string &session_id);

        void updateActivity(const std::string &session_id);
        std::shared_ptr<mb::SSESession> getSession(const std::string &sessionId);

        /** Push a change event to all sessions interested in its topic. */
        void broadcastChange(const json &change_event);
        size_t getSessionCount();

        void start();
        void stop();
        bool isRunning() const;

        static std::function<void(MantisRequest &, MantisResponse &)> handleSSESession();
        static std::function<void(MantisRequest &, MantisResponse &)> handleSSESessionUpdate();

    private:
        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> validateSubTopics(bool is_updating = false);

        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> validateHasAccess();

        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> updateAuthTokenForSSE();

        static std::string generateClientID();

        void cleanupIdleSessions();
    };
}


#endif //MANTISBASE_SSE_H
