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

#if MB_HAS_POSTGRESQL
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
        /** @param app Owning application (used for db access/config). Stored by reference. */
        explicit RealtimeDB(const MantisBase &app);

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

        /**
         * Wake the worker to drain change events immediately (push, not poll).
         * Called by the write path after a mutation commits. No-op if the worker
         * is not running. Thread-safe.
         */
        void notifyChange() const;

    private:
#if MB_HAS_POSTGRESQL
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
        /** @param app Owning application (used for db access/config). Stored by reference. */
        explicit RtDbWorker(const MantisBase &app);

        ~RtDbWorker();

        [[nodiscard]] bool isDbRunning() const;

        void addCallback(const RtCallback &cb);

        void stopWorker();

        /** Signal the worker (SQLite) to wake and drain the change log now. */
        void notify();

    private:
        void run();

        void runSQlite();

#if MB_HAS_POSTGRESQL
        void runPostgreSQL();
#endif

        bool initSQLite();

#if MB_HAS_POSTGRESQL
        bool initPSQL();
#endif

        void pruneChangeLog(int up_to_id); // Delete consumed rows from mb_change_log (SQLite)

        const MantisBase &mApp; // Owning application (injected)
        int last_id = -1; // Last db ID to be queried, only query newer than this
        int m_lastPrunedId = 0; // Highest mb_change_log id already pruned
        std::string last_ts = getCurrentTimestampUTC(); // When last is not set, use timestamp value in UTC

        std::string m_db_type;
        RtCallback m_callback;
        std::atomic<bool> m_running;
        bool m_notified = false; // set by notify(); guarded by mtx
        std::thread th;
        std::mutex mtx;
        std::condition_variable cv;
        std::unique_ptr<soci::session> sql_ro;

#if MB_HAS_POSTGRESQL
        std::unique_ptr<PGconn, decltype(&PQfinish)> psql{nullptr, &PQfinish};
#endif
    };
}

#endif //MANTISBASE_REALTIME_H
