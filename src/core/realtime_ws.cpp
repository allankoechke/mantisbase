#include "../../include/mantisbase/core/ws.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/sse.h"

namespace mb {

    // --- WSMgr ---
    WSMgr::WSMgr(const MantisBase& app) : m_app(app) {}

    void WSMgr::addConnection(const drogon::WebSocketConnectionPtr &conn) {
        std::lock_guard lock(m_mutex);
        m_connTopics[conn] = {};
    }

    void WSMgr::removeConnection(const drogon::WebSocketConnectionPtr &conn) {
        std::lock_guard lock(m_mutex);
        if (auto it = m_connTopics.find(conn); it != m_connTopics.end()) {
            for (const auto &topic : it->second) {
                if (auto tit = m_topicConns.find(topic); tit != m_topicConns.end()) {
                    tit->second.erase(conn);
                    if (tit->second.empty())
                        m_topicConns.erase(tit);
                }
            }
            m_connTopics.erase(it);
        }
    }

    void WSMgr::subscribe(const drogon::WebSocketConnectionPtr &conn,
                           const std::vector<std::string> &topics) {
        std::lock_guard lock(m_mutex);
        auto &connTopics = m_connTopics[conn];
        for (const auto &topic : topics) {
            connTopics.insert(topic);
            m_topicConns[topic].insert(conn);
        }
    }

    void WSMgr::unsubscribe(const drogon::WebSocketConnectionPtr &conn,
                             const std::vector<std::string> &topics) {
        std::lock_guard lock(m_mutex);
        if (auto it = m_connTopics.find(conn); it != m_connTopics.end()) {
            for (const auto &topic : topics) {
                it->second.erase(topic);
                if (auto tit = m_topicConns.find(topic); tit != m_topicConns.end()) {
                    tit->second.erase(conn);
                    if (tit->second.empty())
                        m_topicConns.erase(tit);
                }
            }
        }
    }

    void WSMgr::broadcastChange(const json &change_event) {
        std::lock_guard lock(m_mutex);
        auto formatted = formatEvent(change_event);
        auto msg = formatted.dump();

        for (const auto &[conn, topics] : m_connTopics) {
            if (isInterestedIn(topics, change_event)) {
                conn->send(msg);
            }
        }
    }

    size_t WSMgr::connectionCount() {
        std::lock_guard lock(m_mutex);
        return m_connTopics.size();
    }

    bool WSMgr::isInterestedIn(const std::set<std::string> &topics,
                                const json &change_event) const {
        const auto entity = change_event["entity"].get<std::string>();
        const auto row_id = change_event["row_id"].get<std::string>();

        if (topics.contains(entity))
            return true;
        if (topics.contains(entity + ":" + row_id))
            return true;
        if (topics.contains(entity + ":*"))
            return true;
        return false;
    }

    json WSMgr::formatEvent(const json &change_event) const {
        auto entity = change_event["entity"].get<std::string>();
        auto row_id = change_event["row_id"].get<std::string>();
        auto operation = change_event["type"].get<std::string>();
        toLowerCase(operation);

        json data = (operation == "insert" || operation == "update")
                        ? change_event["new_data"]
                        : nullptr;

        return {
            {"type", "change"},
            {"topic", entity},
            {"action", operation},
            {"timestamp", change_event["timestamp"]},
            {"row_id", row_id},
            {"entity", entity},
            {"data", data}
        };
    }

    // --- RealtimeWSController ---

    void RealtimeWSController::handleNewConnection(
        const drogon::HttpRequestPtr &req,
        const drogon::WebSocketConnectionPtr &conn) {

        // Check env var toggle
        if (const char *env = std::getenv("MB_REALTIME_WS"); env && std::string(env) == "false") {
            conn->shutdown(drogon::CloseCode::kNormalClosure, "WebSocket is disabled");
            return;
        }

        logEntry::info("WebSocket", std::format("New WS connection from {}",
                       conn->peerAddr().toIpPort()));

        auto &wsMgr = m_app.router().sseMgr().wsMgr();
        wsMgr.addConnection(conn);

        json welcome = {{"type", "connected"}, {"message", "WebSocket connected"}};
        conn->send(welcome.dump());
    }

    void RealtimeWSController::handleNewMessage(
        const drogon::WebSocketConnectionPtr &conn,
        std::string &&message,
        const drogon::WebSocketMessageType &type) {

        if (type != drogon::WebSocketMessageType::Text)
            return;

        try {
            auto msg = json::parse(message);
            auto msgType = msg.value("type", "");

            if (msgType == "subscribe") {
                auto topics = msg.value("topics", std::vector<std::string>{});
                if (!topics.empty()) {
                    auto &wsMgr = m_app.router().sseMgr().wsMgr();
                    wsMgr.subscribe(conn, topics);

                    json ack = {{"type", "subscribed"}, {"topics", topics}};
                    conn->send(ack.dump());
                }
            } else if (msgType == "unsubscribe") {
                auto topics = msg.value("topics", std::vector<std::string>{});
                if (!topics.empty()) {
                    auto &wsMgr = m_app.router().sseMgr().wsMgr();
                    wsMgr.unsubscribe(conn, topics);

                    json ack = {{"type", "unsubscribed"}, {"topics", topics}};
                    conn->send(ack.dump());
                }
            } else if (msgType == "ping") {
                json pong = {{"type", "pong"}};
                conn->send(pong.dump());
            }
        } catch (const std::exception &e) {
            json err = {{"type", "error"}, {"message", "Invalid message format"}};
            conn->send(err.dump());
        }
    }

    void RealtimeWSController::handleConnectionClosed(
        const drogon::WebSocketConnectionPtr &conn) {

        logEntry::info("WebSocket", "WS connection closed");

        auto &wsMgr = m_app.router().sseMgr().wsMgr();
        wsMgr.removeConnection(conn);
    }

} // namespace mb
