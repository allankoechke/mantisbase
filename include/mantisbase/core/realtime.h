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

namespace soci {
    class session;
}

namespace mb {
    // Forward declarations
    class MantisBase;
    class Entity;
    class RtDbWorker;

    struct Changelog {
        uint id = 0;
        int timestamp = 0;
        std::string type = "NULL", entity, row_id;
        nlohmann::json old_data = nlohmann::json::object(), new_data = nlohmann::json::object();
    };

    class RealtimeDB {
    public:
        RealtimeDB();
        [[nodiscard]] bool init() const;

        void addDbHooks(const std::string &entity_name) const;
        void addDbHooks(const Entity &entity) const;
        static void addDbHooks(const Entity &entity, const std::shared_ptr<soci::session>& sess) ;

        void dropDbHooks(const std::string &entity_name) const;
        static void dropDbHooks(const std::string &entity_name, const std::shared_ptr<soci::session>& sess) ;

        void runWorker();

    private:
        void workerHandler(const nlohmann::json& items);

        static std::string buildTriggerObject(const Entity &entity, const std::string &action /*"NEW" or "OLD"*/);

        const MantisBase &mApp;
        std::unique_ptr<RtDbWorker> m_rtDbWorker;
    };

    class RtDbWorker {
    public:
        using RtCallback = std::function<void(json)>;

        RtDbWorker();
        ~RtDbWorker();

        void addCallback(const RtCallback &cb);

    private:
        void run();

        int last_id = -1; // Last db ID to be queried, only query newer than this
        std::string last_ts = getCurrentTimestampUTC(); // When last is not set, use timestamp value in UTC

        RtCallback m_callback;
        std::atomic<bool> m_running;
        std::thread th;
        std::mutex mtx;
        std::condition_variable cv;
        std::unique_ptr<soci::session> write_session;
    };
}

#endif //MANTISBASE_REALTIME_H
