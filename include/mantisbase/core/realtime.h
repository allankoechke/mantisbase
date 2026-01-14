#ifndef MANTISBASE_REALTIME_H
#define MANTISBASE_REALTIME_H
#include <string>

#include "mantisbase/mantis.h"

namespace mb {
    // Forward declarations
    class MantisBase;
    class Entity;

    struct Changelog {
        uint id = 0;
        int timestamp = 0;
        std::string type = "NULL", entity, row_id;
        json old_data = json::object(), new_data = json::object();
    };

    class RealtimeDB {
    public:
        RealtimeDB();

        void init() const;

        void addDbHooks(const std::string &entity_name);

        void dropDbHooks(const std::string &entity_name);

    private:
        static std::string buildTriggerObject(const Entity &entity, const std::string &action /*"NEW" or "OLD"*/);

        const MantisBase &mApp;
    };

    class RtDbWorker {
    public:
        using RtCallback = std::function<void(Changelog)>;

        explicit RtDbWorker()
            : m_running(true),
              th(&RtDbWorker::run, this) {
        }

        ~RtDbWorker() {
            m_running.store(false);
            cv.notify_all();
            if (th.joinable())
                th.join();
        }

        void addCallback(const RtCallback &cb) {
            m_callback = cb;
        }

    private:
        void run() {
            int value = 0;
            while (m_running.load()) {
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    cv.wait_for(lock, std::chrono::milliseconds(200));
                }
                Changelog changelog;
                auto sql = MantisBase::instance().db().session();
                // *sql << "SELECT * from mb_change_log LIMIT 1", soci::use(changelog);
                m_callback(changelog);
            }
        }

    private:
        RtCallback m_callback;
        std::atomic<bool> m_running;
        std::thread th;
        std::mutex mtx;
        std::condition_variable cv;
    };
}

#endif //MANTISBASE_REALTIME_H
