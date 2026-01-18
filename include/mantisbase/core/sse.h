#ifndef MANTISBASE_SSE_H
#define MANTISBASE_SSE_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "realtime.h"

namespace mb {
    using json = nlohmann::json;

    struct SSESession {
        std::string session_id;
        std::function<void(const json &)> send_callback;
        std::set<std::string> topics;
        std::atomic<bool> active{true};
        std::chrono::steady_clock::time_point last_activity;

        SSESession(
            std::string id,
            std::function<void(const json &)> cb,
            std::set<std::string> t,
            bool a,
            std::chrono::steady_clock::time_point tp
        ) : session_id(std::move(id)),
            send_callback(std::move(cb)),
            topics(std::move(t)),
            active(a),
            last_activity(tp) {
        }

        SSESession(SSESession &&) = default;
    };

    class SSEMgr {
        std::unordered_map<std::string, SSESession> m_sessions;
        std::mutex m_sessions_mutex;
        std::condition_variable m_cv;
        std::thread m_cleanup_thread;
        std::atomic<bool> m_running{true};

    public:
        SSEMgr() = default;

        ~SSEMgr();

        std::string createSession(
            const std::function<void(const json &)> &callback,
            const std::set<std::string> &initial_topics);

        SSESession &fetchSession(const std::string &session_id);

        void removeSession(const std::string &session_id);

        bool updateTopics(const std::string &session_id,
                          const std::set<std::string> &topics);

        void updateActivity(const std::string &session_id);

        void start();

        void stop();

        static std::function<void(const MantisRequest &, MantisResponse &)> handleSSESession();

        static std::function<void(const MantisRequest &, MantisResponse &)> handleSSESessionUpdate();

    private:
        void cleanupIdleSessions();

        static bool shouldSendToClient(const json &data, const std::set<std::string> &topics);
    };
}


#endif //MANTISBASE_SSE_H
