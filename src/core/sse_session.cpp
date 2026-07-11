#include <utility>

#include "../../include/mantisbase/core/sse.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/utils/uuidv7.h"

mb::SSESession::SSESession(
    std::string client_id,
    const std::set<std::string> &topics,
    drogon::ResponseStreamPtr stream)
    : m_clientID(std::move(client_id)),
      m_topics(topics),
      m_stream(std::move(stream)),
      m_isActive(true),
      m_lastActivity(std::chrono::steady_clock::now()) {
}

bool mb::SSESession::sendEvent(const std::string &eventType, const json &data) {
    if (!m_isActive || !m_stream)
        return false;

    std::string payload;
    payload += "event: " + eventType + "\n";
    payload += "data: " + data.dump() + "\n\n";

    if (!m_stream->send(payload)) {
        m_isActive = false;
        return false;
    }

    updateActivity();
    return true;
}

bool mb::SSESession::isInterestedIn(const json &change_event) const {
    const std::string event_table = change_event["entity"].get<std::string>();
    const std::string event_row_id = change_event["row_id"].get<std::string>();

    if (m_topics.contains(event_table))
        return true;

    std::string specific_topic = event_table + ":" + event_row_id;
    if (m_topics.contains(specific_topic))
        return true;

    std::string wildcard_topic = event_table + ":*";
    if (m_topics.contains(wildcard_topic))
        return true;

    return false;
}

mb::json mb::SSESession::formatEvent(const json &change_event) const {
    auto table = change_event["entity"].get<std::string>();
    auto row_id = change_event["row_id"].get<std::string>();
    auto operation = change_event["type"].get<std::string>();

    toLowerCase(operation);

    std::string matched_topic = table;
    std::string specific_topic = table + ":" + row_id;

    {
        std::lock_guard<std::mutex> lock(m_topicsMutex);
        if (m_topics.contains(specific_topic)) {
            matched_topic = specific_topic;
        }
    }

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

void mb::SSESession::updateTopics(const std::set<std::string> &topics) {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    m_topics = topics;
}

std::chrono::steady_clock::time_point mb::SSESession::getLastActivity() const { return m_lastActivity; }

void mb::SSESession::close() {
    m_isActive = false;
    if (m_stream) {
        m_stream->close();
        m_stream.reset();
    }
}

bool mb::SSESession::isActive() const { return m_isActive; }

const std::string &mb::SSESession::getClientID() const { return m_clientID; }

std::set<std::string> mb::SSESession::getTopics() const {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    return m_topics;
}

void mb::SSESession::setTopics(const std::set<std::string> &topics) {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    m_topics = topics;
}
