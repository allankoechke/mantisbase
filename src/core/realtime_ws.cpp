#include "../../include/mantisbase/core/ws.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/core/api_keys.h"
#include "../../include/mantisbase/core/models/access_rules.h"
#include "../../include/mantisbase/utils/utils.h"

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
        if (strToBool(getEnvOrDefault("MB_DISABLE_REALTIME_WS", "0"))) {
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
        json verification_context;

        // Check for API key
        if (token.starts_with("mb_sk_")) {
            auto key_hash = ApiKeyManager::hashApiKey(token);
            auto key_info = m_app.auth().apiKey().lookupByHash(key_hash);
            if (!key_info.has_value()) {
                conn->shutdown(drogon::CloseCode::kViolation, "Unauthorized");
                return;
            }
            const auto entity_name = key_info.value()["entity_name"].get<std::string>();
            auth_context["entity"] = entity_name;
            auth_context["id"] = key_info.value()["user_id"];
            auth_context["type"] = entity_name == "mb_admins" ? "admin" : "user";
            auth_context["mode"] = "api";
            auth_context["auth_method"] = "api_key";
            verification_context["verified"] = true;
            verification_context["claims"] = {
                {"id", auth_context["id"]},
                {"entity", auth_context["entity"]}
            };
            verification_context["error"] = "";
        } else {
            // JWT token verification
            auto verification = m_app.auth().verifyToken(token);
            if (!verification.at("verified").get<bool>()) {
                conn->shutdown(drogon::CloseCode::kViolation, "Unauthorized");
                return;
            }
            const auto entity_name = verification["claims"]["entity"].get<std::string>();
            auth_context["entity"] = entity_name;
            auth_context["id"] = verification["claims"]["id"];
            auth_context["type"] = entity_name == "mb_admins" ? "admin" : "user";
            auth_context["mode"] = "jwt";
            auth_context["auth_method"] = "jwt";
            verification_context = verification;
        }

        auth_context["user"] = json::object();
        try {
            const auto entity_name = auth_context["entity"].get<std::string>();
            const auto user_id = auth_context["id"].get<std::string>();
            const auto user_entity = m_app.entity(entity_name);
            if (auto user = user_entity.read(user_id); user.has_value()) {
                auto u = user.value();
                u.erase("password");
                auth_context["user"] = u;
            }
        } catch (...) {
        }

        auth_context["verification"] = verification_context;

        m_app.logger().info("WebSocket", std::format("Authenticated WS connection from {}",
                       conn->peerAddr().toIpPort()));

        // Store auth context on the connection for topic authorization
        conn->setContext(std::make_shared<json>(auth_context));

        auto &wsMgr = m_app.router().sseMgr().wsMgr();
        wsMgr.addConnection(conn);

        const json welcome = {{"type", "connected"}, {"message", "WebSocket connected"}};
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
                    auto auth_ctx = conn->getContext<json>();
                    if (!auth_ctx) {
                        json err = {{"type", "error"}, {"message", "Unauthorized"}};
                        conn->send(err.dump());
                        return;
                    }

                    const json verification = auth_ctx->contains("verification")
                                                  ? (*auth_ctx)["verification"]
                                                  : json{{"verified", true}};
                    std::vector<std::string> authorized_topics;
                    for (const auto &topic : topics) {
                        auto entity_name = topic;
                        std::string record_id;
                        if (auto pos = topic.find(':'); pos != std::string::npos) {
                            entity_name = topic.substr(0, pos);
                            record_id = topic.substr(pos + 1);
                        }

                        try {
                            auto entity = m_app.entity(entity_name);
                            auto rule = record_id.empty() ? entity.listRule() : entity.getRule();
                            AccessEvalContext eval_ctx{*auth_ctx, verification, nullptr};
                            if (evaluateAccessRule(rule, eval_ctx) == AccessEvalResult::Allow) {
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
