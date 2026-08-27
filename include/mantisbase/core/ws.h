#ifndef MANTISBASE_WS_H
#define MANTISBASE_WS_H

#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <drogon/WebSocketController.h>
#include <nlohmann/json.hpp>
#include <mantisbase/mantis.h>

#include "realtime_session.h"

namespace mb {
    using json = nlohmann::json;

    class WSMgr {
    public:
        explicit WSMgr(const MantisBase&);
        ~WSMgr() = default;

        void addConnection(const drogon::WebSocketConnectionPtr &conn,
                           RealtimeWsSession session);
        void removeConnection(const drogon::WebSocketConnectionPtr &conn);

        void setTopics(const drogon::WebSocketConnectionPtr &conn,
                       const std::vector<std::string> &topics);
        void unsubscribe(const drogon::WebSocketConnectionPtr &conn,
                         const std::vector<std::string> &topics);

        void broadcastChange(const json &change_event);

        size_t connectionCount();

        void cleanupStaleConnections(const MantisBase &app,
                                     std::chrono::steady_clock::time_point now);

        RealtimeWsSession *getSession(const drogon::WebSocketConnectionPtr &conn);

    private:
        bool isInterestedIn(const std::set<std::string> &topics, const json &change_event) const;
        json formatEvent(const json &change_event) const;
        void removeTopicsFromIndexes(const drogon::WebSocketConnectionPtr &conn,
                                     const std::set<std::string> &topics);

        std::mutex m_mutex;
        std::unordered_map<drogon::WebSocketConnectionPtr, RealtimeWsSession> m_sessions;
        std::unordered_map<drogon::WebSocketConnectionPtr,
                           std::set<std::string>> m_connTopics;
        std::unordered_map<std::string,
                           std::unordered_set<drogon::WebSocketConnectionPtr>> m_topicConns;
        const MantisBase& m_app;
    };

    class RealtimeWSController : public drogon::WebSocketController<RealtimeWSController, false> {
    private:
        const MantisBase& m_app;

    public:
        explicit RealtimeWSController(const MantisBase& app) : m_app(app) {}

        void handleNewMessage(const drogon::WebSocketConnectionPtr &,
                              std::string &&,
                              const drogon::WebSocketMessageType &) override;
        void handleNewConnection(const drogon::HttpRequestPtr &,
                                 const drogon::WebSocketConnectionPtr &) override;
        void handleConnectionClosed(const drogon::WebSocketConnectionPtr &) override;

        WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/api/v1/realtime/ws");
        WS_PATH_LIST_END
    };
}

#endif // MANTISBASE_WS_H
