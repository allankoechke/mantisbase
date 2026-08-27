#include <utility>
#include <vector>
#include <memory>
#include <ranges>

#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/core/ws.h"
#include "../../include/mantisbase/core/realtime_session.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/utils/utils.h"

#include <drogon/drogon.h>

namespace mb {
    static constexpr size_t kMaxSSEConnections = 1024;
    static constexpr size_t kMaxTopicsPerSession = 50;

    namespace {

        std::vector<std::string> topicsJsonToStrings(const json &topics) {
            std::vector<std::string> result;
            for (const auto &topic : topics) {
                auto entity_name = topic["entity"].get<std::string>();
                auto record_id = topic["id"].get<std::string>();
                if (!record_id.empty()) {
                    entity_name = std::format("{}:{}", entity_name, record_id);
                }
                result.push_back(entity_name);
            }
            return result;
        }

        std::set<std::string> vectorToTopicSet(const std::vector<std::string> &topics) {
            return {topics.begin(), topics.end()};
        }

        json topicSetToJsonArray(const std::set<std::string> &topics) {
            json arr = json::array();
            for (const auto &topic : topics) {
                arr.push_back(topic);
            }
            return arr;
        }

        std::string resolveSubscribeToken(MantisRequest &req) {
            const auto &[body, err] = req.getBodyAsJson();
            if (err.empty() && body.contains("token") && body["token"].is_string()) {
                const auto token = trim(body["token"].get<std::string>());
                if (!token.empty()) {
                    return token;
                }
            }

            return resolveRealtimeToken(req.drogonRequest());
        }
    } // namespace

    SSEMgr::SSEMgr(const MantisBase &app)
        : IMantisBase(app),
          m_wsMgr(std::make_unique<WSMgr>(app)) {
    }

    SSEMgr::~SSEMgr() { stop(); }

    void SSEMgr::createRoutes() {
        auto &router = mbApp().router();

        auto sseGetMiddlewares = std::make_shared<std::vector<MiddlewareFn>>(
            std::vector<MiddlewareFn>{validateSubTopics(false)});

        drogon::app().registerHandler(
            "/api/v1/realtime",
            [this, sseGetMiddlewares](const drogon::HttpRequestPtr &req,
                                      std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
                if (strToBool(getEnvOrDefault("MB_DISABLE_REALTIME_SSE", "0"))) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k503ServiceUnavailable);
                    resp->setContentTypeString("application/json");
                    resp->setBody(R"({"status":503,"error":"SSE is disabled","data":{}})");
                    callback(resp);
                    return;
                }

                MantisRequest ma_req{mbApp(), req};
                MantisResponse ma_res{mbApp()};

                auto &preMiddlewares = mbApp().router().preRoutingMiddlewares();
                for (const auto &mw : preMiddlewares) {
                    if (mw(ma_req, ma_res) == HandlerResponse::Handled) {
                        callback(ma_res.drogonResponse());
                        return;
                    }
                }

                for (const auto &mw : *sseGetMiddlewares) {
                    if (mw(ma_req, ma_res) == HandlerResponse::Handled) {
                        callback(ma_res.drogonResponse());
                        return;
                    }
                }

                const auto requested_topics = topicsJsonToStrings(ma_req.getOr<json>("topics", json::array()));
                if (requested_topics.size() > kMaxTopicsPerSession) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k400BadRequest);
                    resp->setContentTypeString("application/json");
                    resp->setBody(R"({"status":400,"error":"Too many topics requested","data":{}})");
                    callback(resp);
                    return;
                }

                const auto auth_snap = resolveRealtimeAuth(mbApp(), resolveRealtimeToken(req));
                const auto access = filterAuthorizedTopics(mbApp(), requested_topics, auth_snap);
                const auto granted_topics = vectorToTopicSet(access.granted);

                auto resp = drogon::HttpResponse::newAsyncStreamResponse(
                    [this, granted_topics, auth_snap, denied = access.denied](drogon::ResponseStreamPtr stream) {
                        const auto client_id = createSession(granted_topics, std::move(stream), auth_snap);

                        if (client_id.empty()) {
                            stream->close();
                            return;
                        }

                        if (auto session = getSession(client_id); session) {
                            json connected = {
                                {"client_id", client_id},
                                {"topics", topicSetToJsonArray(granted_topics)},
                                {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
                            };
                            if (!denied.empty()) {
                                connected["denied"] = denied;
                            }
                            session->sendEvent("connected", connected);
                        }
                    });

                resp->setContentTypeString("text/event-stream");
                resp->addHeader("Cache-Control", "no-cache");
                resp->addHeader("Connection", "keep-alive");
                resp->addHeader("X-Accel-Buffering", "no");
                mbApp().router().applyCorsHeaders(req, resp);
                callback(resp);
            },
            {drogon::Get});

        router.Post("/api/v1/realtime",
                    handleSSESessionUpdate(),
                    {validateSubTopics(true)});

        RealtimeWSController::initPathRouting();
        drogon::app().registerController(std::make_shared<RealtimeWSController>(mbApp()));
    }

    std::string SSEMgr::createSession(const std::set<std::string> &initial_topics,
                                      drogon::ResponseStreamPtr stream,
                                      RealtimeAuthSnapshot auth) {
        std::lock_guard lock(m_sessions_mutex);

        if (m_sessions.size() >= kMaxSSEConnections) {
            logger().warn("SSE Manager", "Maximum SSE connection limit reached");
            return "";
        }

        const std::string client_id = generateRealtimeClientId("rt_sse");
        auto session = std::make_shared<SSESession>(client_id, initial_topics, std::move(stream), std::move(auth));
        m_sessions[client_id] = session;

        logger().info("SSE Manager",
                      std::format("New SSE session: {} (Total: {})", client_id, m_sessions.size()));

        return client_id;
    }

    std::shared_ptr<SSESession> SSEMgr::fetchSession(const std::string &client_id) {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
            return it->second;
        }

        throw MantisException(404, "Session not found!");
    }

    void SSEMgr::removeSession(const std::string &client_id) {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
            it->second->close();
            m_sessions.erase(it);

            logger().info("SSE Manager",
                          std::format("Removed SSE session: {} (Remaining: {})",
                                      client_id, m_sessions.size()));
        }
    }

    void SSEMgr::updateActivity(const std::string &client_id) {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
            it->second->updateActivity();
        }
    }

    std::shared_ptr<SSESession> SSEMgr::getSession(const std::string &client_id) {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);

        if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
            return it->second;
        }
        return nullptr;
    }

    void SSEMgr::broadcastChange(const json &change_event) {
        try {
            {
                std::lock_guard lock(m_sessions_mutex);
                std::vector<std::string> deadSessions;

                for (const auto &[clientId, session] : m_sessions) {
                    if (!session->isActive()) {
                        deadSessions.push_back(clientId);
                        continue;
                    }
                    if (session->isInterestedIn(change_event)) {
                        json formatted = session->formatEvent(change_event);
                        if (!session->sendEvent("change", formatted)) {
                            deadSessions.push_back(clientId);
                        }
                    }
                }

                for (const auto &id : deadSessions) {
                    if (auto it = m_sessions.find(id); it != m_sessions.end()) {
                        it->second->close();
                        m_sessions.erase(it);
                    }
                }
            }

            if (m_wsMgr) {
                m_wsMgr->broadcastChange(change_event);
            }
        } catch (const std::exception &e) {
            logger().info("SSE Manager", "Broadcasting message failed!", e.what());
        }
    }

    size_t SSEMgr::getSessionCount() {
        std::lock_guard lock(m_sessions_mutex);
        return m_sessions.size();
    }

    WSMgr &SSEMgr::wsMgr() const {
        return *m_wsMgr;
    }

    void SSEMgr::start() {
        mbApp().rt().runWorker([this](const json &items) {
            for (const auto &data_item : items) broadcastChange(data_item);
        });

        m_cleanup_thread = std::thread([this] {
            while (m_running.load()) {
                {
                    std::unique_lock lock(m_sessions_mutex);
                    m_cv.wait_for(lock, std::chrono::seconds(kRealtimeCleanupIntervalSeconds));
                }
                cleanupIdleSessions();
            }
        });
    }

    void SSEMgr::stop() {
        mbApp().rt().stopWorker();

        m_running.store(false);
        m_cv.notify_all();

        if (m_cleanup_thread.joinable()) {
            m_cleanup_thread.join();
        }
    }

    bool SSEMgr::isRunning() const { return m_running.load(); }

    std::function<void(MantisRequest &, MantisResponse &)> SSEMgr::handleSSESessionUpdate() {
        return [](MantisRequest &req, MantisResponse &res) {
            const auto topics = req.getOr<json>("topics", json::array());
            const auto client_id = req.getOr<std::string>("client_id", std::string{});

            auto &sse_mgr = req.mbApp().router().sseMgr();

            const auto requested_topics = topicsJsonToStrings(topics);
            if (requested_topics.size() > kMaxTopicsPerSession) {
                res.sendJSON(400, {
                    {"error", "Too many topics requested"},
                    {"data", json::object()},
                    {"status", 400}
                });
                return;
            }

            const auto session = sse_mgr.getSession(client_id);
            if (!session) {
                res.sendJSON(404, {
                    {"error", "Client session not found"},
                    {"data", json::object()},
                    {"status", 404}
                });
                return;
            }

            const auto incoming_auth = resolveRealtimeAuth(req.mbApp(), resolveSubscribeToken(req));
            auto session_auth = session->authSnapshot();
            const auto upgrade = tryUpgradeAuth(session_auth, incoming_auth);

            if (upgrade == AuthUpgradeResult::DeniedDowngrade) {
                res.sendJSON(403, {
                    {"error", "Cannot downgrade authenticated realtime session to guest"},
                    {"data", json::object()},
                    {"status", 403}
                });
                return;
            }

            if (upgrade == AuthUpgradeResult::DeniedSwitch) {
                res.sendJSON(403, {
                    {"error", "Cannot switch authenticated user on an existing realtime session"},
                    {"data", json::object()},
                    {"status", 403}
                });
                return;
            }

            if (upgrade == AuthUpgradeResult::Applied) {
                session->setAuthSnapshot(session_auth);
            }

            const auto access = filterAuthorizedTopics(req.mbApp(), requested_topics, session->authSnapshot());
            const auto granted_topics = vectorToTopicSet(access.granted);
            session->setTopics(granted_topics);

            res.sendJSON(200, {
                {"error", ""},
                {"status", 200},
                {"data", {
                    {"client_id", client_id},
                    {"topics", topicSetToJsonArray(granted_topics)},
                    {"denied", access.denied}
                }}
            });
        };
    }

    std::function<mb::HandlerResponse(MantisRequest &, MantisResponse &)> SSEMgr::validateSubTopics(
        bool is_updating) {
        return [is_updating](MantisRequest &req, const MantisResponse &res) {
            try {
                std::set<std::string> topics;
                if (is_updating) {
                    const auto &[body, err] = req.getBodyAsJson();

                    if (!err.empty()) {
                        res.sendJSON(400, {
                            {"error", err},
                            {"data", json::object()},
                            {"status", 400}
                        });
                        return HandlerResponse::Handled;
                    }

                    if (!body.contains("client_id")) {
                        res.sendJSON(400, {
                            {"error", "Missing client_id in request body."},
                            {"data", json::object()},
                            {"status", 400}
                        });
                        return HandlerResponse::Handled;
                    }

                    const std::string client_id = body["client_id"];
                    if (client_id.empty()) {
                        res.sendJSON(400, {
                            {"error", "Invalid client_id provided"},
                            {"data", json::object()},
                            {"status", 400}
                        });
                        return HandlerResponse::Handled;
                    }

                    if (!body.contains("topics")) {
                        res.sendJSON(400, {
                            {"error", "Missing topics array in request body."},
                            {"data", json::object()},
                            {"status", 400}
                        });
                        return HandlerResponse::Handled;
                    }

                    if (!body["topics"].is_array()) {
                        res.sendJSON(400, {
                            {"error", std::format(
                                "Expected topics array in request body but found `{}`.",
                                body["topics"].dump())},
                            {"data", json::object()},
                            {"status", 400}
                        });
                        return HandlerResponse::Handled;
                    }

                    for (const auto &sub : body["topics"]) {
                        if (auto topic = trim(sub.get<std::string>()); !topic.empty()) {
                            topics.insert(topic);
                        }
                    }

                    req.set("client_id", client_id);
                } else if (req.hasQueryParam("topics")) {
                    const std::string topics_param = req.getQueryParamValue("topics");
                    std::istringstream ss(topics_param);
                    std::string topic;

                    while (std::getline(ss, topic, ',')) {
                        topic = trim(topic);
                        if (!topic.empty()) {
                            topics.insert(topic);
                        }
                    }
                }

                json parsed_topics = json::array();
                for (const auto &topic : topics) {
                    const auto array = splitString(topic, ":");
                    const auto entity_name = array.at(0);
                    const auto record_id = array.size() > 1 && array.at(1) != "*" ? array.at(1) : "";

                    if (!req.mbApp().hasEntity(entity_name)) {
                        res.sendJSON(400, {
                            {"error", "Invalid topic name, expected valid entity name."},
                            {"data", json::object()},
                            {"status", 400}
                        });
                        return HandlerResponse::Handled;
                    }

                    parsed_topics.push_back({
                        {"entity", entity_name},
                        {"id", record_id}
                    });
                }

                req.set("topics", parsed_topics);
            } catch (std::exception &e) {
                std::cerr << e.what() << std::endl;
                res.sendJSON(500, {
                    {"status", 500},
                    {"data", json::object()},
                    {"error", "An internal error occurred."}
                });
                return HandlerResponse::Handled;
            }

            return HandlerResponse::Unhandled;
        };
    }

    void SSEMgr::cleanupIdleSessions() {
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::string> stale_sessions;

        {
            std::lock_guard lock(m_sessions_mutex);

            for (auto &[sessionId, session] : m_sessions) {
                if (!session->isActive()) {
                    stale_sessions.push_back(sessionId);
                    continue;
                }

                if (isAuthExpired(session->authSnapshot(), mbApp())) {
                    session->sendEvent("error", {{"reason", "token_expired"}});
                    stale_sessions.push_back(sessionId);
                    continue;
                }

                const auto connected_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    now - session->getConnectedAt()).count();
                const auto idle_minutes = std::chrono::duration_cast<std::chrono::minutes>(
                    now - session->getLastActivity()).count();

                if (session->getTopics().empty()) {
                    if (connected_seconds > kRealtimeNoTopicGraceSeconds) {
                        stale_sessions.push_back(sessionId);
                    }
                    continue;
                }

                if (idle_minutes > kRealtimeIdleTimeoutMinutes) {
                    stale_sessions.push_back(sessionId);
                    continue;
                }

                session->sendEvent("ping", {{"type", "keepalive"}});
            }

            for (const auto &sessionId : stale_sessions) {
                logger().warn("SSE Manager", std::format("Removing stale session: {}", sessionId));
                if (auto it = m_sessions.find(sessionId); it != m_sessions.end()) {
                    it->second->close();
                    m_sessions.erase(it);
                }
            }

            if (!stale_sessions.empty()) {
                logger().info("SSE Manager",
                              std::format("Cleaned up {} stale sessions (Active: {})",
                                          stale_sessions.size(), m_sessions.size()));
            }
        }

        if (m_wsMgr) {
            m_wsMgr->cleanupStaleConnections(mbApp(), now);
        }
    }
}
