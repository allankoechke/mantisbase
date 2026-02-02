#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/utils/uuidv7.h"

mb::SSESession::SSESession(
    const std::string &client_id,
    const std::set<std::string> &topics)
    : m_clientID(client_id),
      m_topics(topics),
      m_isActive(true),
      m_lastActivity(std::chrono::steady_clock::now()) {
}

void mb::SSESession::queueEvent(const std::string &eventType, const json &data) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.push({eventType, data});
    m_queueCV.notify_one();
}

bool mb::SSESession::waitForEvent(std::string &eventType, json &data, std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_queueMutex);

    if (
        m_queueCV.wait_for(
            lock,
            timeout,
            [this] {
                return !m_eventQueue.empty() || !m_isActive;
            })) {
        if (!m_eventQueue.empty()) {
            auto [event_type, json_data] = m_eventQueue.front();
            m_eventQueue.pop();
            eventType = event_type;
            data = json_data;
            return true;
        }
    }

    return false;
}

bool mb::SSESession::isInterestedIn(const json &change_event) const {
    const std::string event_table = change_event["entity"].get<std::string>();
    const std::string event_row_id = change_event["row_id"].get<std::string>();

    // Check table subscription
    if (m_topics.contains(event_table)) {
        return true;
    }

    // Check specific record subscription
    std::string specific_topic = event_table + ":" + event_row_id;
    if (m_topics.contains(specific_topic)) {
        return true;
    }

    // Check wildcard subscription
    std::string wildcard_topic = event_table + ":*";
    if (m_topics.contains(wildcard_topic)) {
        return true;
    }

    return false;
}

mb::json mb::SSESession::formatEvent(const json &change_event) const {
    auto table = change_event["entity"].get<std::string>();
    auto row_id = change_event["row_id"].get<std::string>();
    auto operation = change_event["type"].get<std::string>();

    toLowerCase(operation);

    // Determine matched topic
    std::string matched_topic = table;
    std::string specific_topic = table + ":" + row_id;

    if (m_topics.contains(specific_topic)) {
        matched_topic = specific_topic;
    }

    // Send new data only (minify response)
    json data = operation=="insert" || operation == "update" ? change_event["new_data"] : nullptr;

    return {
        {"topic", matched_topic},
        {"action", operation},
        {"timestamp", change_event["timestamp"]},
        {"row_id", row_id},
        {"entity", table},
        {"data", data}
    };
}

void mb::SSESession::updateActivity() {
    m_lastActivity = std::chrono::steady_clock::now();
}

void mb::SSESession::updateTopics(std::set<std::string> &topics) {
    m_topics = topics;
}

auto mb::SSESession::getLastActivity() const { return m_lastActivity; }

void mb::SSESession::close() {
    m_isActive = false;
    m_queueCV.notify_all();
}

bool mb::SSESession::isActive() const { return m_isActive; }

const std::string &mb::SSESession::getClientID() const { return m_clientID; }

const std::set<std::string> &mb::SSESession::getTopics() const { return m_topics; }

void mb::SSESession::setTopics(const std::set<std::string> &topics) {
    m_topics = topics;
}

mb::SSEMgr::~SSEMgr() { stop(); }

void mb::SSEMgr::createRoutes() {
    auto &router = MantisBase::instance().router();

    // Realtime endpoints
    router.Get("/api/v1/realtime",
               handleSSESession(),
               {
                   validateSubTopics(false),
                   validateHasAccess(),
                   updateAuthTokenForSSE()
               }
    );

    router.Post("/api/v1/realtime",
                handleSSESessionUpdate(),
                {
                    validateSubTopics(true),
                    validateHasAccess(),
                    updateAuthTokenForSSE()
                }
    );
}

std::string mb::SSEMgr::createSession(const std::set<std::string> &initial_topics) {
    std::lock_guard lock(m_sessions_mutex);

    std::string client_id = generateClientID();
    const auto session = std::make_shared<SSESession>(client_id, initial_topics);
    m_sessions[client_id] = session;

    logEntry::info("SSE Manager",
                   std::format("New SSE session: {} (Total: {})", client_id, m_sessions.size()));

    return client_id;
}

std::shared_ptr<mb::SSESession> mb::SSEMgr::fetchSession(const std::string &client_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
        return it->second;
    }

    throw MantisException(404, "Session not found!");
}

void mb::SSEMgr::removeSession(const std::string &client_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
        it->second->close();
        m_sessions.erase(it);

        logEntry::info("SSE Manager",
                       std::format("Removed SSE session: {} (Remaining: {})",
                                   client_id, m_sessions.size()));
    }
}

void mb::SSEMgr::updateActivity(const std::string &client_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
        it->second->updateActivity();
    }
}

std::shared_ptr<mb::SSESession> mb::SSEMgr::getSession(const std::string &client_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);

    if (const auto it = m_sessions.find(client_id); it != m_sessions.end()) {
        return it->second;
    }
    return nullptr;
}

// Broadcast change event to interested sessions
void mb::SSEMgr::broadcastChange(const json &change_event) {
    try {
        std::lock_guard lock(m_sessions_mutex);

        for (const auto &session: m_sessions | std::views::values) {
            if (session->isInterestedIn(change_event)) {
                json formatted = session->formatEvent(change_event);
                session->queueEvent("change", formatted);
            }
        }
    } catch (const std::exception &e) {
        logEntry::info("SSE Manager", "Broadcasting message failed!", e.what());
    }
}

size_t mb::SSEMgr::getSessionCount() {
    std::lock_guard lock(m_sessions_mutex);
    return m_sessions.size();
}

void mb::SSEMgr::start() {
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

void mb::SSEMgr::stop() {
    MantisBase::instance().rt().stopWorker();

    m_running.store(false);
    m_cv.notify_all();

    if (m_cleanup_thread.joinable()) {
        m_cleanup_thread.join();
    }
}

bool mb::SSEMgr::isRunning() const {return m_running.load();}

std::function<void(mb::MantisRequest &, mb::MantisResponse &)> mb::SSEMgr::handleSSESession() {
    return [](mb::MantisRequest &req, const mb::MantisResponse &res) {
        res.setHeader("Cache-Control", "no-cache");
        res.setHeader("Connection", "keep-alive");
        res.setHeader("Access-Control-Allow-Origin", "*");

        // Parse topics from query parameters
        std::set<std::string> topics;

        for (const auto &topic: req.getOr<json>("topics", json::array())) {
            auto entity_name = topic["entity"].get<std::string>();
            auto record_id = topic["id"].get<std::string>();
            if (!record_id.empty()) entity_name = std::format("{}:{}", entity_name, record_id);
            topics.insert(entity_name);
        }

        if (topics.empty()) {
            res.sendJSON(400,
                         {
                             {"error", "No topics specified to subscribe to."},
                             {"data", json::object()},
                             {"status", 400}
                         }
            );
            return;
        }

        res.getResponse().set_chunked_content_provider(
            "text/event-stream",
            [topics](size_t, const httplib::DataSink &sink) -> bool {
                auto &sse_mgr = MantisBase::instance().router().sseMgr();

                // Helper to send SSE event
                auto sendSSE = [&sink](const std::string &eventType, const json &data) -> bool {
                    if (!sink.is_writable()) {
                        return false;
                    }

                    const std::string message = std::format(
                        "event: {}\ndata: {}\n\n",
                        eventType,
                        data.dump()
                    );

                    return sink.write(message.data(), message.size());
                };

                // Create session
                std::string client_id = sse_mgr.createSession(topics);

                const auto session = sse_mgr.getSession(client_id);
                if (!session) return false;

                // Send connection event
                sendSSE("connected", {
                            {"client_id", client_id},
                            {"topics", topics},
                            {"timestamp", std::time(nullptr)}
                        }
                );

                // Event loop - wait for events or timeout
                while (sink.is_writable() && session->isActive()) {
                    json data;

                    // Wait for event with 30 second timeout
                    if (std::string eventType; session->waitForEvent(eventType, data, std::chrono::seconds(30))) {
                        // Send the event
                        if (!sendSSE(eventType, data)) {
                            break; // Failed to send, connection lost
                        }
                    } else {
                        // Timeout - send ping to keep connection alive
                        if (!sendSSE("ping", {{"timestamp", std::time(nullptr)}})) {
                            break; // Failed to send ping, connection lost
                        }
                    }

                    // Update activity
                    sse_mgr.updateActivity(client_id);
                }

                // Clean up when client disconnects
                sse_mgr.removeSession(client_id);
                return false;
            }
        );
    };
}

std::function<void(mb::MantisRequest &, mb::MantisResponse &)> mb::SSEMgr::handleSSESessionUpdate() {
    return [](mb::MantisRequest &req, mb::MantisResponse &res) {
        // Get the auth var from the context, resort to empty object if it's not set.
        auto topics = req.getOr<json>("topics", json::array());
        auto client_id = req.getOr<json>("client_id", std::string{});

        // Get the auth var from the context, resort to empty object if it's not set.
        auto auth = req.getOr<json>("auth", json::object());

        auto &sse_mgr = MantisBase::instance().router().sseMgr();

        std::set<std::string> new_topics;
        for (const auto &topic: topics) {
            auto entity_name = topic["entity"].get<std::string>();
            auto record_id = topic["id"].get<std::string>();
            if (!record_id.empty()) entity_name = std::format("{}:{}", entity_name, record_id);
            new_topics.insert(entity_name);
        }

        if (const auto session = sse_mgr.getSession(client_id); session) {
            session->setTopics(topics);
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

                // TODO user:id=12344,post:
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
        // Check for access permissions
        // - users or users:* -> list access
        // - users:id -> view access

        // Get the auth var from the context, resort to empty object if it's not set.
        auto topics = req.getOr<json>("topics", json::array());

        // Get the auth var from the context, resort to empty object if it's not set.
        auto auth = req.getOr<json>("auth", json::object());

        // Require at least one valid auth on any table
        auto verification = req.getOr<json>("verification", json::object());

        for (const auto &topic: topics) {
            auto entity_name = topic["entity"].get<std::string>();
            auto record_id = topic["id"].get<std::string>();

            auto entity = MantisBase::instance().entity(entity_name);

            auto rule = record_id.empty() ? entity.listRule() : entity.getRule();

            // For public access, allow access
            if (rule.mode() == "public") continue; // Check remaining topics

            // For empty mode (admin only)
            if (rule.mode().empty()) {
                if (verification.empty()) {
                    // Send auth error
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
                    // Check if verified user object is valid, if not throw auth error
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

                // Send auth error
                res.sendJSON(403, {
                                 {"data", json::object()},
                                 {"status", 403},
                                 {"error", verification["error"]}
                             });
                return HandlerResponse::Handled;
            }

            if (rule.mode() == "auth" || (auth["entity"].is_string()
                                          && auth["entity"].get<std::string>() == "mb_admins")) {
                // Require at least one valid auth on any table
                if (verification.empty()) {
                    // Send auth error
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
                    // Check if verified user object is valid, if not throw auth error
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

                // Send auth error
                res.sendJSON(403, {
                                 {"data", json::object()},
                                 {"status", 403},
                                 {"error", verification["error"]}
                             });
                return HandlerResponse::Handled;
            }

            if (rule.mode() == "custom") {
                // Evaluate expression
                const std::string expr = rule.expr();

                // Token map variables for evaluation
                TokenMap vars;

                // Add `auth` data to the TokenMap
                vars["auth"] = auth;

                // Request Token Map
                json req_obj;
                req_obj["remoteAddr"] = req.getRemoteAddr();
                req_obj["remotePort"] = req.getRemotePort();
                req_obj["localAddr"] = req.getLocalAddr();
                req_obj["localPort"] = req.getLocalPort();
                req_obj["body"] = json::object();

                try {
                    if (req.getMethod() == "POST" && !req.getBody().empty()) {
                        // Parse request body and add it to the request TokenMap
                        req_obj["body"] = req.getBodyAsJson();
                    }
                } catch (...) {
                }

                // Add the request map to the vars
                vars["req"] = req_obj;

                // If expression evaluation returns true, lets return allowing execution
                // continuation. Else, we'll craft an error response.
                if (Expr::eval(expr, vars))
                    continue;

                // Evaluation yielded false, return generic access denied error
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
        auto idle_time = std::chrono::duration_cast<std::chrono::minutes>(
            now - session->getLastActivity()
        ).count();

        // Remove sessions idle for more than 10 minutes
        // or having no topics subscribed to.
        if (idle_time > 10 || session->getTopics().empty()) {
            stale_sessions.push_back(sessionId);
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
