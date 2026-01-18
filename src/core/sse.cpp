#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/utils/uuidv7.h"

mb::SSEMgr::~SSEMgr() { stop(); }

std::string mb::SSEMgr::createSession(const std::function<void(const json &)> &callback,
                                      const std::set<std::string> &initial_topics) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);

    // Generate unique session ID
    const auto session_id = generate_uuidv7();

    m_sessions.try_emplace(
        session_id,
        session_id,
        callback,
        initial_topics,
        true,
        std::chrono::steady_clock::now()
    );

    return session_id;
}

mb::SSESession & mb::SSEMgr::fetchSession(const std::string &session_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (const auto it = m_sessions.find(session_id); it != m_sessions.end()) {
        return it->second;
    }

    throw MantisException(404, "Session not found!");
}

void mb::SSEMgr::removeSession(const std::string &session_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (const auto it = m_sessions.find(session_id); it != m_sessions.end()) {
        it->second.active = false;
        m_sessions.erase(it);
    }
}

bool mb::SSEMgr::updateTopics(const std::string &session_id, const std::set<std::string> &topics) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (const auto it = m_sessions.find(session_id); it != m_sessions.end()) {
        it->second.topics = topics;
        it->second.last_activity = std::chrono::steady_clock::now();
        return true;
    }

    return false;
}

void mb::SSEMgr::updateActivity(const std::string &session_id) {
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    if (auto it = m_sessions.find(session_id); it != m_sessions.end()) {
        it->second.last_activity = std::chrono::steady_clock::now();
    }
}

void mb::SSEMgr::start() {
    MantisBase::instance().rt().runWorker([this](const json &items) {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        for (const auto &data_item: items) {
            for (auto &session: m_sessions | std::views::values) {
                if (session.active && shouldSendToClient(data_item, session.topics)) {
                    session.send_callback(data_item);
                    session.last_activity = std::chrono::steady_clock::now();
                }
            }
        }
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

std::function<void(const mb::MantisRequest &, mb::MantisResponse &)> mb::SSEMgr::handleSSESession() {
    return [](const mb::MantisRequest &req, mb::MantisResponse &res) {
        res.setHeader("Cache-Control", "no-cache");
        res.setHeader("Connection", "keep-alive");

        // Parse topics from query parameters
        std::set<std::string> topics;
        if (req.hasQueryParam("topics")) {
            // TODO validate topics ...
            for (int i = 0; i < req.getQueryParamValueCount("topics"); i++) {
                topics.insert(req.getQueryParamValue("topics", i));
            }
        }

        res.getResponse().set_chunked_content_provider(
            "text/event-stream",
            [topics](size_t, const httplib::DataSink &sink) -> bool {
                auto send_callback = [&](const json &data) {
                    if (sink.is_writable()) {
                        const std::string message = "data: " + data.dump() + "\n\n";
                        if (!sink.write(message.data(), message.size())) {
                            // TODO ...
                        }
                    }
                };

                auto &sse_mgr = MantisBase::instance().router().sseMgr();

                // Create session and get session ID
                auto session_id = sse_mgr.createSession(send_callback, topics);

                // Send session ID as first message
                const json session_response = {
                    {"session_id", session_id},
                    {"signal", "MB_CONNECTION"}
                };

                const std::string init_msg = "data: " + session_response.dump() + "\n\n";
                auto _ = sink.write(init_msg.data(), init_msg.size());

                // Keep connection alive
                while (sink.is_writable()) {
                    sse_mgr.updateActivity(session_id);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                // Clean up when client disconnects
                sse_mgr.removeSession(session_id);
                return false;
            }
        );
    };
}

std::function<void(const mb::MantisRequest &, mb::MantisResponse &)> mb::SSEMgr::handleSSESessionUpdate() {
    return [](const mb::MantisRequest &req, mb::MantisResponse &res) {
        std::string session_id;
        std::set<std::string> new_topics;

        // Try to parse as JSON first
        try {
            const auto &[body, err] = req.getBodyAsJson();
            if (!err.empty()) throw std::runtime_error(err);

            if (body.contains("session_id")) {
                session_id = body["session_id"];
            }
            if (body.contains("topics")) {
                for (const auto &sub: body["topics"]) {
                    new_topics.insert(sub.get<std::string>());
                }
            }
        } catch (...) {
            // Fall back to query parameters
            if (req.hasQueryParam("session_id"))
                session_id = req.getQueryParamValue("session_id");

            if (req.hasQueryParam("topics")) {
                session_id = req.getQueryParamValue("topics");
                for (int i = 0; i < req.getQueryParamValueCount("topics"); i++) {
                    new_topics.insert(req.getQueryParamValue("topics", i));
                }
            }
        }

        if (session_id.empty()) {
            res.sendJSON(400, {
                             {"error", "Missing session_id"},
                             {"data", json::object()},
                             {"status", 400}
                         });
            return;
        }


        auto &sse_mgr = MantisBase::instance().router().sseMgr();

        if (sse_mgr.updateTopics(session_id, new_topics)) {
            res.sendJSON(200, {
                             {"error", ""},
                             {"data", {
                                 {"session_id", session_id },
                                 {"topics", sse_mgr.fetchSession(session_id).topics }
                             }},
                             {"status", 200 }
                         });
            return;
        }
            res.sendJSON(404, {
                             {"error", "Session not found"},
                             {"data", json::object()},
                             {"status", 404}
                         });
    };
}

void mb::SSEMgr::cleanupIdleSessions() {
    const auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::minutes(10);

    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if (now - it->second.last_activity > timeout) {
            // Send disconnect signal
            json disconnect_msg = {{"signal", "MB_DISCONNECTED"}};
            it->second.send_callback(disconnect_msg);
            it->second.active = false;
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

bool mb::SSEMgr::shouldSendToClient(const json &data, const std::set<std::string> &topics) {
    for (const auto &topic: topics) {
        if (topic == "*") return true;
        if (topic == data["entity"].get<std::string>()) return true;
        // TODO add subscription to records by id
    }

    return false;
}
