/**
 * @file ws.h
 * @brief WebSocket realtime manager and controller for `/api/v1/realtime/ws`.
 *
 * Mirrors the SSE two-phase flow:
 * - **Connect** (`handleNewConnection`): optional auth, welcome `{ type, client_id, topics: [] }`.
 * - **Subscribe** (`handleNewMessage`): optional `token`, topic replace, ack with `denied[]`.
 *
 * Also supports `{ "type": "auth", "token": "..." }` as a token-only upgrade alias.
 *
 * @see realtime_session.h
 * @see sse.h
 * @see realtime.h
 */

#ifndef MANTISBASE_WS_H
#define MANTISBASE_WS_H

#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <drogon/WebSocketController.h>
#include <nlohmann/json.hpp>
#include <mantisbase/mantis.h>

#include "realtime_session.h"

namespace mb {
    using json = nlohmann::json;

    /**
     * @brief Tracks WebSocket connections, topic indexes, and broadcasts change events.
     *
     * Thread-safe. One instance is owned by @ref SSEMgr and shares the SSE cleanup loop
     * via @ref cleanupStaleConnections.
     */
    class WSMgr {
    public:
        explicit WSMgr(const MantisBase&);
        ~WSMgr() = default;

        /** Register a new connection and its session snapshot. Closes with 1008 if at capacity. */
        void addConnection(const drogon::WebSocketConnectionPtr &conn,
                           RealtimeWsSession session);

        /** Remove connection from topic indexes and session map. */
        void removeConnection(const drogon::WebSocketConnectionPtr &conn);

        /** Replace the connection's topic set (subscribe replaces, does not merge). */
        void setTopics(const drogon::WebSocketConnectionPtr &conn,
                       const std::vector<std::string> &topics);

        /** Remove specific topics from a connection without affecting others. */
        void unsubscribe(const drogon::WebSocketConnectionPtr &conn,
                         const std::vector<std::string> &topics);

        /** Deliver a formatted change event to all connections subscribed to the matching topic. */
        void broadcastChange(const json &change_event);

        /** @return Number of active WebSocket sessions. */
        size_t connectionCount();

        /**
         * @brief Close expired, idle, or connect-only sessions.
         * @param app Application instance (auth expiry checks).
         * @param now Current steady clock time (shared with SSE cleanup).
         */
        void cleanupStaleConnections(const MantisBase &app,
                                     std::chrono::steady_clock::time_point now);

        /** @return Mutable session for a connection, or `nullptr` if unknown. */
        RealtimeWsSession *getSession(const drogon::WebSocketConnectionPtr &conn);

    private:
        bool isInterestedIn(const std::set<std::string> &topics, const json &change_event) const;
        json formatEvent(const json &change_event) const;
        void removeTopicsFromIndexes(const drogon::WebSocketConnectionPtr &conn,
                                     const std::set<std::string> &topics);

        std::mutex m_mutex;
        std::unordered_map<drogon::WebSocketConnectionPtr, RealtimeWsSession> m_sessions;
        std::unordered_map<drogon::WebSocketConnectionPtr,
                           std::set<std::string>> m_connTopics;
        std::unordered_map<std::string,
                           std::unordered_set<drogon::WebSocketConnectionPtr>> m_topicConns;
        const MantisBase& m_app;
    };

    /**
     * @brief Drogon WebSocket controller for the realtime endpoint.
     *
     * Registered at `WS /api/v1/realtime/ws`. Disabled when `MB_DISABLE_REALTIME_WS` is truthy.
     */
    class RealtimeWSController : public drogon::WebSocketController<RealtimeWSController, false> {
    private:
        const MantisBase& m_app;

    public:
        explicit RealtimeWSController(const MantisBase& app) : m_app(app) {}

        /** Handle subscribe, unsubscribe, ping, and auth-upgrade messages. */
        void handleNewMessage(const drogon::WebSocketConnectionPtr &,
                              std::string &&,
                              const drogon::WebSocketMessageType &) override;

        /** Optional auth at connect; sends welcome with `client_id` and empty `topics`. */
        void handleNewConnection(const drogon::HttpRequestPtr &,
                                 const drogon::WebSocketConnectionPtr &) override;

        /** Tear down session state when the client disconnects. */
        void handleConnectionClosed(const drogon::WebSocketConnectionPtr &) override;

        WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/api/v1/realtime/ws");
        WS_PATH_LIST_END
    };
}

#endif // MANTISBASE_WS_H
