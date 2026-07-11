#ifndef MANTISBASE_WS_H
#define MANTISBASE_WS_H

#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <drogon/WebSocketController.h>
#include <nlohmann/json.hpp>

namespace mb {
    using json = nlohmann::json;

    class WSMgr {
    public:
        WSMgr() = default;
        ~WSMgr() = default;

        void addConnection(const drogon::WebSocketConnectionPtr &conn);
        void removeConnection(const drogon::WebSocketConnectionPtr &conn);

        void subscribe(const drogon::WebSocketConnectionPtr &conn,
                       const std::vector<std::string> &topics);
        void unsubscribe(const drogon::WebSocketConnectionPtr &conn,
                         const std::vector<std::string> &topics);

        void broadcastChange(const json &change_event);

        size_t connectionCount();

    private:
        bool isInterestedIn(const std::set<std::string> &topics, const json &change_event) const;
        json formatEvent(const json &change_event) const;

        std::mutex m_mutex;
        std::unordered_map<drogon::WebSocketConnectionPtr,
                           std::set<std::string>> m_connTopics;
        std::unordered_map<std::string,
                           std::unordered_set<drogon::WebSocketConnectionPtr>> m_topicConns;
    };

    class RealtimeWSController : public drogon::WebSocketController<RealtimeWSController> {
    public:
        void handleNewMessage(const drogon::WebSocketConnectionPtr &,
                              std::string &&,
                              const drogon::WebSocketMessageType &) override;
        void handleNewConnection(const drogon::HttpRequestPtr &,
                                 const drogon::WebSocketConnectionPtr &) override;
        void handleConnectionClosed(const drogon::WebSocketConnectionPtr &) override;

        WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/api/v1/realtime/ws")
        WS_PATH_LIST_END
    };
}

#endif // MANTISBASE_WS_H
