/**
 * @file realtime.h
 * @brief Realtime database change detection for SQLite and PostgreSQL.
 *
 * Provides live change notifications for entity tables so that SSE (Server-Sent Events)
 * and other consumers can broadcast insert, update, and delete events. Supported backends:
 * - **SQLite**: Polling-based change detection.
 * - **PostgreSQL**: LISTEN/NOTIFY with triggers.
 *
 * Used in conjunction with sse.h to power the /api/v1/realtime SSE endpoint.
 * @see sse.h
 */

#ifndef MANTISBASE_REALTIME_H
#define MANTISBASE_REALTIME_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "mantisbase/mantis.h"
#include "nlohmann/json.hpp"

#if MANTIS_HAS_POSTGRESQL
#include <soci/postgresql/soci-postgresql.h>
#include <libpq-fe.h>
#endif

namespace soci {
    class session;
}

namespace mb {
    // Forward declarations
    class MantisBase;
    class Entity;
    class RtDbWorker;

    using json = nlohmann::json;

    /** Callback invoked with a batch of change events (each event is a JSON object). */
    using RtCallback = std::function<void(const json &)>;

    /**
     * Realtime database change detection and notification.
     * Initializes DB-specific hooks (triggers for PostgreSQL, polling for SQLite)
     * and runs a worker that invokes the registered callback with change events.
     */
    class RealtimeDB {
    public:
        RealtimeDB();

        /** Initialize realtime for the current database backend. Must be called after DB is ready. */
        [[nodiscard]] bool init() const;

        /** Register change hooks for an entity by name. */
        void addDbHooks(const std::string &entity_name) const;

        /** Register change hooks for an entity. */
        void addDbHooks(const Entity &entity) const;

        /** Register change hooks for an entity on a given session (static, for schema creation). */
        static void addDbHooks(const Entity &entity, const std::shared_ptr<soci::session> &sess);

        /** Remove change hooks for an entity by name. */
        void dropDbHooks(const std::string &entity_name) const;

        /** Remove change hooks for an entity on a given session (static). */
        static void dropDbHooks(const std::string &entity_name, const std::shared_ptr<soci::session> &sess);

        /** Start the realtime worker; callback receives change events. */
        void runWorker(const RtCallback &callback);

        /** Stop the realtime worker. */
        void stopWorker() const;

    private:
#if MANTIS_HAS_POSTGRESQL
        // Create the notification trigger function
        static void createNotifyFunction(soci::session &sql);
#endif

        static std::string buildTriggerObject(const Entity &entity, const std::string &action /*"NEW" or "OLD"*/);

        const MantisBase &mApp;
        std::unique_ptr<RtDbWorker> m_rtDbWorker;
    };

    /** Internal worker that polls (SQLite) or listens (PostgreSQL) for DB changes. */
    class RtDbWorker {
    public:
        RtDbWorker();

        ~RtDbWorker();

        [[nodiscard]] bool isDbRunning() const;

        void addCallback(const RtCallback &cb);

        void stopWorker();

    private:
        void run();

        void runSQlite();

#if MANTIS_HAS_POSTGRESQL
        void runPostgreSQL();
#endif

        bool initSQLite();

#if MANTIS_HAS_POSTGRESQL
        bool initPSQL();
#endif

        int last_id = -1; // Last db ID to be queried, only query newer than this
        std::string last_ts = getCurrentTimestampUTC(); // When last is not set, use timestamp value in UTC

        std::string m_db_type;
        RtCallback m_callback;
        std::atomic<bool> m_running;
        std::thread th;
        std::mutex mtx;
        std::condition_variable cv;
        std::unique_ptr<soci::session> sql_ro;

#if MANTIS_HAS_POSTGRESQL
        std::unique_ptr<PGconn, decltype(&PQfinish)> psql{nullptr, &PQfinish};
#endif
    };
}

#endif //MANTISBASE_REALTIME_H
