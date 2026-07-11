#include <utility>

#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/core/ws.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/utils/uuidv7.h"

#include <drogon/drogon.h>

namespace mb {
    SSEMgr::SSEMgr()
        : m_wsMgr(std::make_unique<WSMgr>()) {}

    SSEMgr::~SSEMgr() { stop(); }

    void SSEMgr::createRoutes() {
        auto &router = MantisBase::instance().router();

        // SSE GET endpoint — registers directly with Drogon for async streaming
        auto getMiddlewares = std::vector<MiddlewareFn>{
            validateSubTopics(false),
            validateHasAccess(),
            updateAuthTokenForSSE()
        };

        auto sseGetMiddlewares = std::make_shared<std::vector<MiddlewareFn>>(std::move(getMiddlewares));

        drogon::app().registerHandler(
            "/api/v1/realtime",
            [this, sseGetMiddlewares](const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

                // Check env var toggle
                if (const char *env = std::getenv("MB_REALTIME_SSE"); env && std::string(env) == "false") {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k503ServiceUnavailable);
                    resp->setContentTypeString("application/json");
                    resp->setBody(R"({"status":503,"error":"SSE is disabled","data":{}})");
                    callback(resp);
                    return;
                }

                MantisRequest ma_req{req};
                MantisResponse ma_res{};

                // Run global pre-routing middlewares
                auto &preMiddlewares = MantisBase::instance().router().preRoutingMiddlewares();
                for (const auto &mw : preMiddlewares) {
                    if (mw(ma_req, ma_res) == HandlerResponse::Handled) {
                        callback(ma_res.drogonResponse());
                        return;
                    }
                }

                // Run SSE-specific middlewares
                for (const auto &mw : *sseGetMiddlewares) {
                    if (mw(ma_req, ma_res) == HandlerResponse::Handled) {
                        callback(ma_res.drogonResponse());
                        return;
                    }
                }

                // Parse topics from middleware context
                auto topics = ma_req.getOr<json>("topics", json::array());
                std::set<std::string> topicSet;
                for (const auto &topic : topics) {
                    auto entity_name = topic["entity"].get<std::string>();
                    auto record_id = topic["id"].get<std::string>();
                    if (!record_id.empty())
                        entity_name = std::format("{}:{}", entity_name, record_id);
                    topicSet.insert(entity_name);
                }

                // Create async stream response for SSE
                auto resp = drogon::HttpResponse::newAsyncStreamResponse(
                    [this, topicSet](drogon::ResponseStreamPtr stream) {
                        auto clientId = createSession(topicSet, std::move(stream));

                        // Send the initial connected event through the session
                        if (auto session = getSession(clientId); session) {
                            json connected = {
                                {"client_id", clientId},
                                {"topics", topicSet}
                            };
                            session->sendEvent("connected", connected);
                        }
                    });

                resp->setContentTypeString("text/event-stream");
                resp->addHeader("Cache-Control", "no-cache");
                resp->addHeader("Connection", "keep-alive");
                resp->addHeader("X-Accel-Buffering", "no");
                resp->addHeader("Access-Control-Allow-Origin", "*");
                callback(resp);
            },
            {drogon::Get});

        // POST /api/v1/realtime — update topics for existing session
        router.Post("/api/v1/realtime",
                    handleSSESessionUpdate(),
                    {
                        validateSubTopics(true),
                        validateHasAccess(),
                        updateAuthTokenForSSE()
                    }
        );
    }

    std::string SSEMgr::createSession(const std::set<std::string> &initial_topics,
                                       drogon::ResponseStreamPtr stream) {
        std::lock_guard lock(m_sessions_mutex);

        std::string client_id = generateClientID();
        auto session = std::make_shared<SSESession>(client_id, initial_topics, std::move(stream));
        m_sessions[client_id] = session;

        logEntry::info("SSE Manager",
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

            logEntry::info("SSE Manager",
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
            // Broadcast to SSE sessions
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

            // Broadcast to WebSocket connections
            if (m_wsMgr) {
                m_wsMgr->broadcastChange(change_event);
            }
        } catch (const std::exception &e) {
            logEntry::info("SSE Manager", "Broadcasting message failed!", e.what());
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
        MantisBase::instance().rt().runWorker([this](const json &items) {
            for (const auto &data_item: items) broadcastChange(data_item);
        });

        // Start cleanup thread for idle connections
        m_cleanup_thread = std::thread([this] {
            while (m_running.load()) {
                {
                    std::unique_lock lock(m_sessions_mutex);
                    m_cv.wait_for(lock, std::chrono::minutes(1));
                }
                cleanupIdleSessions();
            }
        });
    }

    void SSEMgr::stop() {
        MantisBase::instance().rt().stopWorker();

        m_running.store(false);
        m_cv.notify_all();

        if (m_cleanup_thread.joinable()) {
            m_cleanup_thread.join();
        }
    }

    bool SSEMgr::isRunning() const {return m_running.load();}

    static std::function<void(mb::MantisRequest &, mb::MantisResponse &)> handleSSESessionUpdate() {
        return [](mb::MantisRequest &req, mb::MantisResponse &res) {
            auto topics = req.getOr<json>("topics", json::array());
            auto client_id = req.getOr<std::string>("client_id", std::string{});
            auto auth = req.getOr<json>("auth", json::object());
            auto verification = req.getOr<json>("verification", json::object());

            auto &sse_mgr = MantisBase::instance().router().sseMgr();

            std::set<std::string> new_topics;
            for (const auto &topic: topics) {
                auto entity_name = topic["entity"].get<std::string>();
                auto record_id = topic["id"].get<std::string>();
                if (!record_id.empty()) entity_name = std::format("{}:{}", entity_name, record_id);
                new_topics.insert(entity_name);
            }

            if (const auto session = sse_mgr.getSession(client_id); session) {
                session->setTopics(new_topics);
                res.sendJSON(200, {
                                 {"error", ""},
                                 {
                                     "data", {
                                         {"client_id", client_id},
                                         {"topics", new_topics}
                                     }
                                 },
                                 {"status", 200}
                             }
                             );
                return;
            }

            res.sendJSON(404, {
                             {"error", "Client session not found"},
                             {"data", json::object()},
                             {"status", 404}
                         });
        };
    }

    std::function<mb::HandlerResponse(mb::MantisRequest &, mb::MantisResponse &)> mb::SSEMgr::validateSubTopics(
        bool is_updating) {
        return [is_updating](MantisRequest &req, const MantisResponse &res) {
            try {
                std::set<std::string> topics;
                // Parse JSON body when updating the sub topics
                if (is_updating) {
                    // Try to parse as JSON first
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

                    std::string client_id = body["client_id"];

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
                                         {
                                             "error", std::format("Expected topics array in request body but found `{}`.",
                                                                  body["topics"].dump())
                                         },
                                         {"data", json::object()},
                                         {"status", 400}
                                     });
                        return HandlerResponse::Handled;
                    }

                    for (const auto &sub: body["topics"]) {
                        if (auto topic = trim(sub.get<std::string>()); !topic.empty())
                            topics.insert(topic);
                    }

                    req.set("client_id", client_id);
                }
                // Parse URL Query params otherwise
                else {
                    if (req.hasQueryParam("topics")) {
                        const std::string topics_param = req.getQueryParamValue("topics");

                        // Split by comma
                        std::istringstream ss(topics_param);
                        std::string topic;

                        while (std::getline(ss, topic, ',')) {
                            // Trim whitespace
                            topic = trim(topic);

                            if (!topic.empty()) {
                                topics.insert(topic);
                            }
                        }
                    }
                }

                json _topics = json::array();

                for (const auto &topic: topics) {
                    auto array = splitString(topic, ":");
                    auto entity_name = array.at(0);
                    auto record_id = array.size() > 1 && array.at(1) != "*" ? array.at(1) : "";

                    if (!MantisBase::instance().hasEntity(entity_name)) {
                        res.sendJSON(400, {
                                         {"error", "Invalid topic name, expected valid entity name."},
                                         {"data", json::object()},
                                         {"status", 400}
                                     });
                        return HandlerResponse::Handled;
                    }

                    _topics.push_back({
                        {"entity", entity_name},
                        {"id", record_id}
                    });
                }

                // Store decoded topics on context ...
                req.set("topics", _topics);
            } catch (std::exception &e) {
                std::cerr << e.what() << std::endl;
                res.sendJSON(500, {
                    {"status", 500},
                    {"data", json::object()},
                    {"error", e.what()}
                });
            }

            return HandlerResponse::Unhandled;
        };
    }

    std::function<mb::HandlerResponse(mb::MantisRequest &, mb::MantisResponse &)> mb::SSEMgr::validateHasAccess() {
        return [](MantisRequest &req, const MantisResponse &res) -> HandlerResponse {
            auto topics = req.getOr<json>("topics", json::array());
            auto auth = req.getOr<json>("auth", json::object());
            auto verification = req.getOr<json>("verification", json::object());

            for (const auto &topic: topics) {
                auto entity_name = topic["entity"].get<std::string>();
                auto record_id = topic["id"].get<std::string>();

                auto entity = MantisBase::instance().entity(entity_name);

                auto rule = record_id.empty() ? entity.listRule() : entity.getRule();

                // For public access, allow access
                if (rule.mode() == "public") continue;

                // For empty mode (admin only)
                if (rule.mode().empty()) {
                    if (verification.empty()) {
                        res.sendJSON(403, {
                                         {"data", json::object()},
                                         {"status", 403},
                                         {
                                             "error",
                                             std::format("Admin auth required to access record(s) in `{}` entity.",
                                                         entity_name)
                                         }
                                     });
                        return HandlerResponse::Handled;
                    }

                    if (verification.contains("verified") &&
                        verification["verified"].is_boolean() &&
                        verification["verified"].get<bool>()) {
                        if (auth["user"].is_null() || !auth["user"].is_object()) {
                            res.sendJSON(403, {
                                             {"data", json::object()},
                                             {"status", 403},
                                             {"error", "Auth user not found!"}
                                         });

                            return HandlerResponse::Handled;
                        }

                        continue;
                        }

                    res.sendJSON(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", verification["error"]}
                                 });
                    return HandlerResponse::Handled;
                }

                if (rule.mode() == "auth" || (auth["entity"].is_string()
                                              && auth["entity"].get<std::string>() == "mb_admins")) {
                    if (verification.empty()) {
                        res.sendJSON(403, {
                                         {"data", json::object()},
                                         {"status", 403},
                                         {"error", "Auth required to access this resource!"}
                                     });
                        return HandlerResponse::Handled;
                    }

                    if (verification.contains("verified") &&
                        verification["verified"].is_boolean() &&
                        verification["verified"].get<bool>()) {
                        if (auth["user"].is_null() || !auth["user"].is_object()) {
                            res.sendJSON(403, {
                                             {"data", json::object()},
                                             {"status", 403},
                                             {"error", "Auth user not found!"}
                                         });
                            return HandlerResponse::Handled;
                        }

                        continue;
                        }

                    res.sendJSON(403, {
                                     {"data", json::object()},
                                     {"status", 403},
                                     {"error", verification["error"]}
                                 });
                    return HandlerResponse::Handled;
                                              }

                if (rule.mode() == "custom") {
                    const std::string expr = rule.expr();

                    json vars = json::object();
                    vars["auth"] = auth;

                    json req_obj;
                    req_obj["remoteAddr"] = req.getRemoteAddr();
                    req_obj["remotePort"] = req.getRemotePort();
                    req_obj["localAddr"] = req.getLocalAddr();
                    req_obj["localPort"] = req.getLocalPort();
                    req_obj["body"] = json::object();

                    try {
                        if (req.getMethod() == "POST" && !req.getBody().empty()) {
                            req_obj["body"] = req.getBodyAsJson();
                        }
                    } catch (...) {
                    }

                    vars["req"] = req_obj;

                    if (Expr::eval(expr, vars))
                        continue;

                    json response;
                    response["status"] = 403;
                    response["data"] = json::object();
                    response["error"] = "Access denied!";
                    res.sendJSON(403, response);

                    return HandlerResponse::Handled;
                }

                res.sendJSON(403, {
                                 {"status", 403},
                                 {"data", json::object()},
                                 {"error", std::format("Access denied, entity `{}` access rule unknown.", entity_name)}
                             });
                return HandlerResponse::Handled;
            }

            return HandlerResponse::Unhandled;
        };
    }

    std::function<mb::HandlerResponse(mb::MantisRequest &, mb::MantisResponse &)> mb::SSEMgr::updateAuthTokenForSSE() {
        return [](MantisRequest &req, const MantisResponse &res) {
            // TODO update session token here ...
            return HandlerResponse::Unhandled;
        };
    }

    std::string mb::SSEMgr::generateClientID() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        return std::format("sse_{}_{}{}", now, counter++, generateShortId(5));
    }

    void mb::SSEMgr::cleanupIdleSessions() {
        std::lock_guard lock(m_sessions_mutex);

        const auto now = std::chrono::steady_clock::now();
        std::vector<std::string> stale_sessions;

        for (auto &[sessionId, session]: m_sessions) {
            if (!session->isActive()) {
                stale_sessions.push_back(sessionId);
                continue;
            }

            auto idle_time = std::chrono::duration_cast<std::chrono::minutes>(
                now - session->getLastActivity()
            ).count();

            if (idle_time > 10 || session->getTopics().empty()) {
                stale_sessions.push_back(sessionId);
            } else {
                // Send keepalive ping to active sessions
                json ping = {{"type", "keepalive"}};
                session->sendEvent("ping", ping);
            }
        }

        for (const auto &sessionId: stale_sessions) {
            logEntry::warn("SSE Manager",
                           std::format("Removing stale session: {}", sessionId));

            if (auto it = m_sessions.find(sessionId); it != m_sessions.end()) {
                it->second->close();
                m_sessions.erase(it);
            }
        }

        if (!stale_sessions.empty()) {
            logEntry::info("SSE Manager",
                           std::format("Cleaned up {} stale sessions (Active: {})",
                                       stale_sessions.size(), m_sessions.size()));
        }
    }
}
