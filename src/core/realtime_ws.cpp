#include "../../include/mantisbase/core/ws.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/core/api_keys.h"

namespace mb {

    static constexpr size_t kMaxWSConnections = 1024;

    // --- WSMgr ---
    WSMgr::WSMgr(const MantisBase& app) : m_app(app) {}

    void WSMgr::addConnection(const drogon::WebSocketConnectionPtr &conn) {
        std::lock_guard lock(m_mutex);
        if (m_connTopics.size() >= kMaxWSConnections) {
            conn->shutdown(drogon::CloseCode::kViolation, "Connection limit reached");
            return;
        }
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

        if (data.is_object() && data.contains("password"))
            data.erase("password");

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

        // Authenticate: extract token from query param or Authorization header
        std::string token;
        if (req->getParameter("token").length() > 0) {
            token = req->getParameter("token");
        } else if (req->getHeader("Authorization").length() > 0) {
            auto auth_header = req->getHeader("Authorization");
            if (auth_header.starts_with("Bearer ")) {
                token = auth_header.substr(7);
            }
        }

        if (token.empty()) {
            conn->shutdown(drogon::CloseCode::kViolation, "Unauthorized");
            return;
        }

        json auth_context;

        // Check for API key
        if (token.starts_with("mb_sk_")) {
            auto key_hash = ApiKeyManager::hashApiKey(token);
            auto key_info = m_app.auth().apiKey().lookupByHash(key_hash);
            if (!key_info.has_value()) {
                conn->shutdown(drogon::CloseCode::kViolation, "Unauthorized");
                return;
            }
            auth_context["entity"] = key_info.value()["entity_name"];
            auth_context["id"] = key_info.value()["user_id"];
        } else {
            // JWT token verification
            auto verification = m_app.auth().verifyToken(token);
            if (!verification.at("verified").get<bool>()) {
                conn->shutdown(drogon::CloseCode::kViolation, "Unauthorized");
                return;
            }
            auth_context["entity"] = verification["claims"]["entity"];
            auth_context["id"] = verification["claims"]["id"];
        }

        m_app.logger().info("WebSocket", std::format("Authenticated WS connection from {}",
                       conn->peerAddr().toIpPort()));

        // Store auth context on the connection for topic authorization
        conn->setContext(std::make_shared<json>(auth_context));

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
                    // Validate each topic against the entity's access rules
                    auto ctx = conn->getContext<json>();
                    if (!ctx) {
                        json err = {{"type", "error"}, {"message", "Unauthorized"}};
                        conn->send(err.dump());
                        return;
                    }

                    auto user_entity = ctx->value("entity", "");
                    std::vector<std::string> authorized_topics;
                    for (const auto &topic : topics) {
                        // Extract entity name from topic (format: "entity_name" or "entity_name:row_id")
                        auto entity_name = topic;
                        if (auto pos = topic.find(':'); pos != std::string::npos) {
                            entity_name = topic.substr(0, pos);
                        }

                        try {
                            auto entity = m_app.entity(entity_name);
                            auto rule = entity.listRule();
                            // Admin-only ("") and "custom" rules are only opened up to
                            // admins; everything else needs at least an authenticated user,
                            // which the connection handshake already established.
                            if (rule.mode() == "public" || rule.mode() == "auth" ||
                                user_entity == "mb_admins") {
                                authorized_topics.push_back(topic);
                            }
                        } catch (...) {
                            // Entity does not exist — skip topic
                        }
                    }

                    if (!authorized_topics.empty()) {
                        auto &wsMgr = m_app.router().sseMgr().wsMgr();
                        wsMgr.subscribe(conn, authorized_topics);
                    }

                    json ack = {{"type", "subscribed"}, {"topics", authorized_topics}};
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

        m_app.logger().info("WebSocket", "WS connection closed");

        auto &wsMgr = m_app.router().sseMgr().wsMgr();
        wsMgr.removeConnection(conn);
    }

} // namespace mb
