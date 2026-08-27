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
 * @see realtime_session.h
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
#include "realtime_session.h"
#include "types.h"

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
        std::chrono::steady_clock::time_point m_connectedAt;

        RealtimeAuthSnapshot m_auth;

    public:
        SSESession(std::string client_id,
                   const std::set<std::string> &topics,
                   drogon::ResponseStreamPtr stream,
                   RealtimeAuthSnapshot auth = makeGuestAuthSnapshot());

        bool sendEvent(const std::string &eventType, const json &data);
        bool isInterestedIn(const json &change_event) const;
        json formatEvent(const json &change_event) const;

        void updateActivity();
        void updateTopics(const std::set<std::string> &topics);

        std::chrono::steady_clock::time_point getLastActivity() const;
        std::chrono::steady_clock::time_point getConnectedAt() const;

        void close();

        bool isActive() const;

        const std::string &getClientID() const;

        std::set<std::string> getTopics() const;
        void setTopics(const std::set<std::string> &topics);

        RealtimeAuthSnapshot &authSnapshot();
        [[nodiscard]] const RealtimeAuthSnapshot &authSnapshot() const;
        void setAuthSnapshot(RealtimeAuthSnapshot auth);
    };

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

        void createRoutes();
        std::string createSession(const std::set<std::string> &initial_topics,
                                  drogon::ResponseStreamPtr stream,
                                  RealtimeAuthSnapshot auth = makeGuestAuthSnapshot());

        std::shared_ptr<SSESession> fetchSession(const std::string &session_id);
        void removeSession(const std::string &session_id);

        void updateActivity(const std::string &session_id);
        std::shared_ptr<mb::SSESession> getSession(const std::string &sessionId);

        void broadcastChange(const json &change_event);
        size_t getSessionCount();

        WSMgr &wsMgr() const;

        void start();
        void stop();
        bool isRunning() const;

    private:
        static std::function<void(MantisRequest &, MantisResponse &)> handleSSESessionUpdate();

        static std::function<HandlerResponse(MantisRequest &, MantisResponse &)> validateSubTopics(bool is_updating = false);

        void cleanupIdleSessions();
    };
}


#endif //MANTISBASE_SSE_H
