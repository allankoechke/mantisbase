#ifndef MANTISBASE_REALTIME_H
#define MANTISBASE_REALTIME_H

#include <condition_variable>
#include <functional>
#include <libpq-fe.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "mantisbase/mantis.h"
#include "nlohmann/json.hpp"

#ifdef MANTIS_HAS_POSTGRESQL
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

    using RtCallback = std::function<void(const json &)>;

    class RealtimeDB {
    public:
        RealtimeDB();

        [[nodiscard]] bool init() const;

        void addDbHooks(const std::string &entity_name) const;

        void addDbHooks(const Entity &entity) const;

        static void addDbHooks(const Entity &entity, const std::shared_ptr<soci::session> &sess);

        void dropDbHooks(const std::string &entity_name) const;

        static void dropDbHooks(const std::string &entity_name, const std::shared_ptr<soci::session> &sess);

        void runWorker(const RtCallback &callback);

        void stopWorker() const;

    private:
#ifdef MANTIS_HAS_POSTGRESQL
        // Create the notification trigger function
        static void createNotifyFunction(soci::session &sql);
#endif

        static std::string buildTriggerObject(const Entity &entity, const std::string &action /*"NEW" or "OLD"*/);

        const MantisBase &mApp;
        std::unique_ptr<RtDbWorker> m_rtDbWorker;
    };

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

#ifdef MANTIS_HAS_POSTGRESQL
        void runPostgreSQL();
#endif

        bool initSQLite();

#ifdef MANTIS_HAS_POSTGRESQL
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

#ifdef MANTIS_HAS_POSTGRESQL
        std::unique_ptr<PGconn, decltype(&PQfinish)> psql{nullptr, &PQfinish};
#endif
    };
}

#endif //MANTISBASE_REALTIME_H
