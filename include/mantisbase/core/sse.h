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

    // Session that queues events to be sent
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
        void queueEvent(const std::string &eventType, const json &data);

        // Wait for next event with timeout
        bool waitForEvent(std::string &eventType, json &data,
                          std::chrono::milliseconds timeout);

        bool isInterestedIn(const json &change_event) const;

        json formatEvent(const json &change_event) const;

        void updateActivity();

        void updateTopics(std::set<std::string> &topics);

        auto getLastActivity() const;

        void close();

        bool isActive() const;

        const std::string &getClientID() const;

        const std::set<std::string> &getTopics() const;

        void setTopics(const std::set<std::string> &topics);
    };

    class SSEMgr {
        std::unordered_map<std::string, std::shared_ptr<SSESession>> m_sessions;
        std::mutex m_sessions_mutex;
        std::condition_variable m_cv;
        std::thread m_cleanup_thread;
        std::atomic<bool> m_running{true};

    public:
        SSEMgr() = default;
        ~SSEMgr();

        static void createRoutes();
        std::string createSession(const std::set<std::string> &initial_topics);

        std::shared_ptr<SSESession> fetchSession(const std::string &session_id);
        void removeSession(const std::string &session_id);

        void updateActivity(const std::string &session_id);
        std::shared_ptr<mb::SSESession> getSession(const std::string &sessionId);

        void broadcastChange(const json &change_event);
        size_t getSessionCount();

        void start();
        void stop();

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
