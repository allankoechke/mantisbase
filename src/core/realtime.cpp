#include "../../include/mantisbase/core/realtime.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/exceptions.h"
#include "../../include/mantisbase/core/database.h"

#include <soci/soci.h>
#include "soci/sqlite3/soci-sqlite3.h"

mb::RealtimeDB::RealtimeDB()
: mApp(mb::MantisBase::instance()) {}

bool mb::RealtimeDB::init() const {
    try {
        const auto &sql = mApp.db().session();

        // Create rt changelog table in the AUDIT database
        *sql << R"(
            CREATE TABLE IF NOT EXISTS mb_change_log (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP,
                type        TEXT NOT NULL,
                entity      TEXT NOT NULL,
                row_id      TEXT NOT NULL,
                old_data    TEXT,
                new_data    TEXT
            )
        )";

        // Create indexes on the audit database
        *sql << "CREATE INDEX IF NOT EXISTS idx_change_log_id ON mb_change_log(id)";
        *sql << "CREATE INDEX IF NOT EXISTS idx_change_log_timestamp ON mb_change_log(timestamp)";
        return true;
    } catch (std::exception &e) {
        LogOrigin::dbCritical(e.what());
    }

    return false;
}

void mb::RealtimeDB::addDbHooks(const std::string &entity_name) const {
    if (!mApp.hasEntity(entity_name)) {
        throw MantisException(400,
                              std::format("Expected a valid existing entity name, but `{}` was given instead.",
                                          entity_name));
    }

    // Get entity object
    const auto entity = mApp.entity(entity_name);
    addDbHooks(entity);
}

void mb::RealtimeDB::addDbHooks(const Entity &entity) const {
    // Get session instance
    const auto &sql = mApp.db().session();
    addDbHooks(entity, sql);
}

void mb::RealtimeDB::addDbHooks(const Entity &entity, const std::shared_ptr<soci::session> &sess) {
    logEntry::debug("Realtime Mgr", std::format("Creating Db Hooks on `{}`", entity.name()));

    const auto entity_name = entity.name();

    // First drop any existing hooks
    dropDbHooks(entity_name, sess);

    auto old_obj = buildTriggerObject(entity, "OLD");
    auto new_obj = buildTriggerObject(entity, "NEW");

    // Create INSERT trigger
    *sess << std::format(
        "CREATE TRIGGER mb_{}_insert_trigger AFTER INSERT ON {} "
        "\n\tBEGIN "
        "\n\t\tINSERT INTO mb_change_log(type, entity, row_id, new_data) "
        "\n\t\tVALUES ('INSERT', '{}', NEW.id, {}); "
        "\n\tEND;", entity_name, entity_name, entity_name, new_obj);

    // Create UPDATE trigger
    *sess << std::format(
        "CREATE TRIGGER mb_{}_update_trigger AFTER UPDATE ON {} "
        "\n\tBEGIN "
        "\n\t\tINSERT INTO mb_change_log(type, entity, row_id, old_data, new_data) "
        "\n\t\tVALUES ('UPDATE', '{}', NEW.id, {}, {}); "
        "\n\tEND;", entity_name, entity_name, entity_name, old_obj, new_obj);

    // Create DELETE trigger
    *sess << std::format(
        "CREATE TRIGGER mb_{}_delete_trigger AFTER DELETE ON {} "
        "\n\tBEGIN "
        "\n\t\tINSERT INTO mb_change_log(type, entity, row_id, old_data) "
        "\n\t\tVALUES ('DELETE', '{}', OLD.id, {}); "
        "\n\tEND;", entity_name, entity_name, entity_name, old_obj);
}

void mb::RealtimeDB::dropDbHooks(const std::string &entity_name) const {
    const auto sql = mApp.db().session();
    dropDbHooks(entity_name, sql);
}

void mb::RealtimeDB::dropDbHooks(const std::string &entity_name, const std::shared_ptr<soci::session> &sess) {
    if (!EntitySchema::isValidEntityName(entity_name)) {
        throw MantisException(400, "Invalid Entity name.");
    }

    *sess << std::format("DROP TRIGGER IF EXISTS mb_{}_insert_trigger", entity_name);
    *sess << std::format("DROP TRIGGER IF EXISTS mb_{}_update_trigger", entity_name);
    *sess << std::format("DROP TRIGGER IF EXISTS mb_{}_delete_trigger", entity_name);
}

void mb::RealtimeDB::runWorker(const RtCallback& callback) {
    if (!m_rtDbWorker) {
        m_rtDbWorker = std::make_unique<RtDbWorker>();
        m_rtDbWorker->addCallback(callback);
    }
}

void mb::RealtimeDB::stopWorker() const {
    if (m_rtDbWorker) {
        m_rtDbWorker->stopWorker();
    }
}

void mb::RealtimeDB::workerHandler(const nlohmann::json &items) {
    std::cout << "\n\n-- NEW DATA\n\t> " << items.dump(4) << std::endl;
}

std::string mb::RealtimeDB::buildTriggerObject(const Entity &entity, const std::string &action) {
    std::stringstream ss;
    ss << "json_object(";
    int i = 0;
    for (const auto &field: entity.fields()) {
        auto name = field["name"].get<std::string>();
        ss << "'" << name << "', " << action << "." << name;
        if (i < entity.fields().size() - 1) {
            ss << ", ";
        }

        i++;
    }
    ss << ")";

    return ss.str();
}

mb::RtDbWorker::RtDbWorker()
    : m_running(true),
      th(&RtDbWorker::run, this) {
    // Connect to main db
    auto audit_db_path = joinPaths(MantisBase::instance().dataDir(), "mantis.db").string();

    try {
        auto sqlite_conn_str = std::format(
            "db={} timeout=30 mode=ro shared_cache=true synchronous=normal", audit_db_path);

        write_session = std::make_unique<soci::session>(soci::sqlite3, sqlite_conn_str);

        // Enable WAL mode for better concurrency
        *write_session << "PRAGMA journal_mode=WAL";
        *write_session << "PRAGMA synchronous=NORMAL";
    } catch (std::exception &e) {
        logEntry::critical(
            "Realtime Db Worker",
            "Failed to connect to mantis.db database for auditing",
            e.what()
            );
    }
}

mb::RtDbWorker::~RtDbWorker() {
    stopWorker();

    // Close audit session
    if (write_session && write_session->is_connected()) {
        write_session->close();
    }
}

void mb::RtDbWorker::addCallback(const RtCallback &cb) {
    m_callback = cb;
}

void mb::RtDbWorker::stopWorker() {
    m_running.store(false);
    cv.notify_all();

    if (th.joinable())
        th.join();
}

void mb::RtDbWorker::run() {
    int emptyPollCount = 0;
    auto sleep_for = std::chrono::milliseconds(500);

    while (m_running.load()) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait_for(lock, sleep_for);
        }

        try {
            soci::rowset<soci::row> row_set
                    = last_id < 0
                          ? (
                              write_session->prepare <<
                              "SELECT id, timestamp, type, entity, row_id, old_data, new_data from mb_change_log "
                              "WHERE timestamp > :ts ORDER BY id ASC LIMIT 100"
                              , soci::use(last_ts)
                          )
                          : (
                              write_session->prepare <<
                              "SELECT id, timestamp, type, entity, row_id, old_data, new_data from mb_change_log "
                              "WHERE id > :last_id ORDER BY id ASC LIMIT 100"
                              , soci::use(last_id)
                          );

            json res = json::array();

            for (const auto &row: row_set) {
                auto old_data = row.get_indicator(5) == soci::i_null ? "" : row.get<std::string>(5);
                auto new_data = row.get_indicator(6) == soci::i_null ? "" : row.get<std::string>(6);

                auto od = tryParseJsonStr(old_data, json::object()).value();
                auto nd = tryParseJsonStr(new_data, json::object()).value();

                res.push_back({
                    {"id", row.get<int>(0)},
                    {"timestamp", tmToStr(row.get<std::tm>(1))},
                    {"type", row.get<std::string>(2)},
                    {"entity", row.get<std::string>(3)},
                    {"row_id", row.get<std::string>(4)},
                    {"old_data", od},
                    {"new_data", nd}
                });
            }

            if (!res.empty()) {
                // Get last element's `id`
                last_id = res.at(res.size() - 1)["id"].get<int>();

                emptyPollCount=0;
                sleep_for = std::chrono::milliseconds(100);
            } else {
                emptyPollCount++;

                // Adaptive backoff: slow down when idle
                if (emptyPollCount > 5) {
                    sleep_for = std::chrono::milliseconds(100);
                }

                if (emptyPollCount > 20) {
                    sleep_for = std::chrono::milliseconds(500);
                }

                if (emptyPollCount > 50) {
                    sleep_for = std::chrono::milliseconds(1000);
                }

                if (emptyPollCount > 100) {
                    sleep_for = std::chrono::milliseconds(3000);
                }

                if (emptyPollCount > 300) {
                    sleep_for = std::chrono::milliseconds(5000);
                }
            }

            // Call only when we have data
            if (m_callback && !res.empty())
                m_callback(res);

        } catch (std::exception &e) {
            logEntry::critical("Realtime Db Worker", e.what());
        }
    }
}
