#include <utility>

#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/utils/uuidv7.h"

namespace {
    /** Strip token from auth before storing in session to avoid leaking credentials in memory. */
    mb::json sanitizeAuthForStorage(const mb::json &auth) {
        mb::json out = auth;
        out["token"] = nullptr; // Never persist the raw token in session
        return out;
    }
}

mb::SSESession::SSESession(
    std::string client_id,
    const std::set<std::string> &topics,
    const json &auth,
    const json &verification)
    : m_clientID(std::move(client_id)),
      m_topics(topics),
      m_isActive(true),
      m_lastActivity(std::chrono::steady_clock::now()) {
}

void mb::SSESession::queueEvent(const std::string &eventType, const json &data) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_eventQueue.emplace(eventType, data);
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

std::chrono::steady_clock::time_point mb::SSESession::getLastActivity() const { return m_lastActivity; }

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
