#include "../../include/mantisbase/core/ws.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/core/realtime_session.h"
#include "../../include/mantisbase/utils/utils.h"

namespace mb {

    static constexpr size_t kMaxWSConnections = 1024;

    WSMgr::WSMgr(const MantisBase& app) : m_app(app) {}

    void WSMgr::addConnection(const drogon::WebSocketConnectionPtr &conn,
                                RealtimeWsSession session) {
        std::lock_guard lock(m_mutex);
        if (m_sessions.size() >= kMaxWSConnections) {
            conn->shutdown(drogon::CloseCode::kViolation, "Connection limit reached");
            return;
        }

        m_sessions[conn] = std::move(session);
        m_connTopics[conn] = {};
    }

    void WSMgr::removeConnection(const drogon::WebSocketConnectionPtr &conn) {
        std::lock_guard lock(m_mutex);
        if (auto it = m_connTopics.find(conn); it != m_connTopics.end()) {
            removeTopicsFromIndexes(conn, it->second);
            m_connTopics.erase(it);
        }
        m_sessions.erase(conn);
    }

    void WSMgr::removeTopicsFromIndexes(const drogon::WebSocketConnectionPtr &conn,
                                        const std::set<std::string> &topics) {
        for (const auto &topic : topics) {
            if (auto tit = m_topicConns.find(topic); tit != m_topicConns.end()) {
                tit->second.erase(conn);
                if (tit->second.empty()) {
                    m_topicConns.erase(tit);
                }
            }
        }
    }

    void WSMgr::setTopics(const drogon::WebSocketConnectionPtr &conn,
                          const std::vector<std::string> &topics) {
        std::lock_guard lock(m_mutex);
        auto &connTopics = m_connTopics[conn];
        removeTopicsFromIndexes(conn, connTopics);
        connTopics.clear();

        for (const auto &topic : topics) {
            connTopics.insert(topic);
            m_topicConns[topic].insert(conn);
        }

        if (auto it = m_sessions.find(conn); it != m_sessions.end()) {
            it->second.topics = connTopics;
            touchWsSession(it->second);
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
                    if (tit->second.empty()) {
                        m_topicConns.erase(tit);
                    }
                }
            }

            if (auto sit = m_sessions.find(conn); sit != m_sessions.end()) {
                sit->second.topics = it->second;
                touchWsSession(sit->second);
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
        return m_sessions.size();
    }

    RealtimeWsSession *WSMgr::getSession(const drogon::WebSocketConnectionPtr &conn) {
        std::lock_guard lock(m_mutex);
        if (auto it = m_sessions.find(conn); it != m_sessions.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void WSMgr::cleanupStaleConnections(const MantisBase &app,
                                        const std::chrono::steady_clock::time_point now) {
        struct StaleConn {
            drogon::WebSocketConnectionPtr conn;
            bool token_expired{false};
        };
        std::vector<StaleConn> stale;

        {
            std::lock_guard lock(m_mutex);
            for (auto &[conn, session] : m_sessions) {
                if (isAuthExpired(session.auth, app)) {
                    stale.push_back({conn, true});
                    continue;
                }

                const auto connected_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    now - session.connected_at).count();
                const auto idle_minutes = std::chrono::duration_cast<std::chrono::minutes>(
                    now - session.last_activity).count();

                if (session.topics.empty()) {
                    if (connected_seconds > kRealtimeNoTopicGraceSeconds) {
                        stale.push_back({conn, false});
                    }
                    continue;
                }

                if (idle_minutes > kRealtimeIdleTimeoutMinutes) {
                    stale.push_back({conn, false});
                }
            }
        }

        for (const auto &entry : stale) {
            if (entry.token_expired) {
                json err = {{"type", "error"}, {"message", "token_expired"}};
                entry.conn->send(err.dump());
                entry.conn->shutdown(drogon::CloseCode::kViolation, "token_expired");
            } else {
                entry.conn->shutdown(drogon::CloseCode::kNormalClosure, "idle_timeout");
            }
            removeConnection(entry.conn);
        }
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

    namespace {

        RealtimeWsSession *wsSessionFromConn(const drogon::WebSocketConnectionPtr &conn) {
            if (auto ctx = conn->getContext<RealtimeWsSession>(); ctx) {
                return ctx.get();
            }
            return nullptr;
        }

        void syncConnContext(const drogon::WebSocketConnectionPtr &conn, const RealtimeWsSession &session) {
            conn->setContext(std::make_shared<RealtimeWsSession>(session));
        }

        RealtimeAuthSnapshot resolveWsMessageAuth(const MantisBase &app, const json &msg,
                                                    RealtimeWsSession *session) {
            if (msg.contains("token") && msg["token"].is_string()) {
                const auto token = trim(msg["token"].get<std::string>());
                if (!token.empty()) {
                    return resolveRealtimeAuth(app, token);
                }
            }
            return session ? session->auth : makeGuestAuthSnapshot();
        }

        void handleSubscribe(const MantisBase &app,
                             const drogon::WebSocketConnectionPtr &conn,
                             const json &msg) {
            auto session_ctx = conn->getContext<RealtimeWsSession>();
            if (!session_ctx) {
                json err = {{"type", "error"}, {"message", "Session not initialized"}};
                conn->send(err.dump());
                return;
            }

            RealtimeWsSession session = *session_ctx;
            touchWsSession(session);

            const auto incoming_auth = resolveWsMessageAuth(app, msg, &session);
            auto session_auth = session.auth;
            const auto upgrade = tryUpgradeAuth(session_auth, incoming_auth);

            if (upgrade == AuthUpgradeResult::DeniedDowngrade ||
                upgrade == AuthUpgradeResult::DeniedSwitch) {
                json err = {{"type", "error"}, {"message", "Auth upgrade denied"}};
                conn->send(err.dump());
                return;
            }

            if (upgrade == AuthUpgradeResult::Applied) {
                session.auth = session_auth;
            }

            auto topics = msg.value("topics", std::vector<std::string>{});
            const auto access = filterAuthorizedTopics(app, topics, session.auth);

            auto &wsMgr = app.router().sseMgr().wsMgr();
            wsMgr.setTopics(conn, access.granted);

            if (auto *mgr_session = wsMgr.getSession(conn); mgr_session) {
                session.auth = mgr_session->auth;
                session.topics = mgr_session->topics;
                session.client_id = mgr_session->client_id;
                session.connected_at = mgr_session->connected_at;
                session.last_activity = mgr_session->last_activity;
                mgr_session->auth = session.auth;
            }

            syncConnContext(conn, session);

            json ack = {
                {"type", "subscribed"},
                {"topics", access.granted},
                {"denied", access.denied}
            };
            conn->send(ack.dump());
        }
    }

    void RealtimeWSController::handleNewConnection(
        const drogon::HttpRequestPtr &req,
        const drogon::WebSocketConnectionPtr &conn) {

        if (strToBool(getEnvOrDefault("MB_DISABLE_REALTIME_WS", "0"))) {
            conn->shutdown(drogon::CloseCode::kNormalClosure, "WebSocket is disabled");
            return;
        }

        RealtimeWsSession session;
        session.client_id = generateRealtimeClientId("rt_ws");
        session.auth = resolveRealtimeAuth(m_app, resolveRealtimeToken(req));
        session.connected_at = std::chrono::steady_clock::now();
        session.last_activity = session.connected_at;

        m_app.logger().info("WebSocket", std::format("WS connection {} from {}",
                       session.client_id, conn->peerAddr().toIpPort()));

        conn->setContext(std::make_shared<RealtimeWsSession>(session));

        auto &wsMgr = m_app.router().sseMgr().wsMgr();
        wsMgr.addConnection(conn, session);

        json welcome = {
            {"type", "connected"},
            {"client_id", session.client_id},
            {"topics", json::array()}
        };
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

            if (auto *session = wsSessionFromConn(conn); session) {
                touchWsSession(*session);
            }

            if (msgType == "subscribe" || msgType == "auth") {
                handleSubscribe(m_app, conn, msg);
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
        } catch (const std::exception &) {
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
