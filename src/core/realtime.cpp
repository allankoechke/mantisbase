#include "../../include/mantisbase/core/realtime.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/exceptions.h"
#include "../../include/mantisbase/core/database.h"

#include <soci/soci.h>
#include "soci/sqlite3/soci-sqlite3.h"

mb::RealtimeDB::RealtimeDB(const MantisBase &app)
    : IMantisBase(app) {}

bool mb::RealtimeDB::init() const {
    try {
        const auto &sql = mbApp().db().session();

        // Create rt changelog table in the AUDIT database
        if (sql->get_backend_name() == "sqlite3") {
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
        }

#if MB_HAS_POSTGRESQL
        else {
            *sql << R"(
            CREATE TABLE IF NOT EXISTS mb_change_log (
                id          SERIAL PRIMARY KEY,
                timestamp   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                type        TEXT NOT NULL,
                entity      TEXT NOT NULL,
                row_id      TEXT NOT NULL,
                old_data    TEXT,
                new_data    TEXT
            )
            )";
        }
#endif

        // Create indexes on the audit database
        *sql << "CREATE INDEX IF NOT EXISTS idx_change_log_timestamp ON mb_change_log(timestamp)";
        *sql << "CREATE INDEX IF NOT EXISTS idx_change_log_type ON mb_change_log(type)";
        *sql << "CREATE INDEX IF NOT EXISTS idx_change_log_entity ON mb_change_log(entity)";
        *sql << "CREATE INDEX IF NOT EXISTS idx_change_log_row_id ON mb_change_log(row_id)";

#if MB_HAS_POSTGRESQL
        // For PostgreSQL, create the hook function
        if (sql->get_backend_name() == "postgresql") {
            createNotifyFunction(mbApp(), *sql);
        }
#endif

        return true;
    } catch (std::exception &e) {
        mbApp().logger().critical("Database", e.what());
    }

    return false;
}

void mb::RealtimeDB::addDbHooks(const std::string &entity_name) const {
    if (!mbApp().hasEntity(entity_name)) {
        throw MantisException(400,
                              std::format("Expected a valid existing entity name, but `{}` was given instead.",
                                          entity_name));
    }

    // Get entity object
    const auto entity = mbApp().entity(entity_name);
    addDbHooks(entity);
}

void mb::RealtimeDB::addDbHooks(const Entity &entity) const {
    // Get session instance
    const auto &sql = mbApp().db().session();
    addDbHooks(entity, sql);
}

void mb::RealtimeDB::addDbHooks(const Entity &entity, const std::shared_ptr<soci::session> &sess) {
    entity.mbApp().logger().debug("Realtime Mgr", std::format("Creating Db Hooks on `{}`", entity.name()));

    // Validate up front: the entity name is interpolated into trigger DDL below.
    const auto entity_name = sqlIdentifier(entity.name());

    // First drop any existing hooks
    dropDbHooks(entity_name, sess);

    if (const auto db_type = sess->get_backend_name(); db_type == "sqlite3") {
        auto old_obj = buildTriggerObject(entity, "OLD");
        auto new_obj = buildTriggerObject(entity, "NEW");

        // Create INSERT trigger
        *sess << std::format(
            "CREATE TRIGGER mb_{0}_insert_trigger AFTER INSERT ON {0} "
            "\n\tBEGIN "
            "\n\t\tINSERT INTO mb_change_log(type, entity, row_id, new_data) "
            "\n\t\tVALUES ('INSERT', '{0}', NEW.id, {1}); "
            "\n\tEND;", entity_name, new_obj);

        // Create UPDATE trigger
        *sess << std::format(
            "CREATE TRIGGER mb_{0}_update_trigger AFTER UPDATE ON {0} "
            "\n\tBEGIN "
            "\n\t\tINSERT INTO mb_change_log(type, entity, row_id, old_data, new_data) "
            "\n\t\tVALUES ('UPDATE', '{0}', NEW.id, {1}, {2}); "
            "\n\tEND;", entity_name, old_obj, new_obj);

        // Create DELETE trigger
        *sess << std::format(
            "CREATE TRIGGER mb_{0}_delete_trigger AFTER DELETE ON {0} "
            "\n\tBEGIN "
            "\n\t\tINSERT INTO mb_change_log(type, entity, row_id, old_data) "
            "\n\t\tVALUES ('DELETE', '{0}', OLD.id, {1}); "
            "\n\tEND;", entity_name, old_obj);
    }

#if MB_HAS_POSTGRESQL
    else if (db_type == "postgresql") {
        // Create INSERT trigger
        *sess << std::format(R"(
            CREATE TRIGGER mb_{0}_insert_notify
            AFTER INSERT ON {0}
            FOR EACH ROW
            EXECUTE FUNCTION mb_notify_changes()
        )", entity_name);

        // Create UPDATE trigger
        *sess << std::format(R"(
            CREATE TRIGGER mb_{0}_update_notify
            AFTER UPDATE ON {0}
            FOR EACH ROW
            EXECUTE FUNCTION mb_notify_changes()
        )", entity_name);

        // Create DELETE trigger
        *sess << std::format(R"(
            CREATE TRIGGER mb_{0}_delete_notify
            AFTER DELETE ON {0}
            FOR EACH ROW
            EXECUTE FUNCTION mb_notify_changes()
        )", entity_name);
    }
#endif

    else
        throw MantisException(400,
                              std::format("Realtime Mgr: Database Hooks not implemented for db type `{}`", db_type));
}

void mb::RealtimeDB::dropDbHooks(const std::string &entity_name) const {
    const auto& sql = mbApp().db().session();
    dropDbHooks(entity_name, sql);
}

void mb::RealtimeDB::dropDbHooks(const std::string &entity_name, const std::shared_ptr<soci::session> &sess) {
    if (!EntitySchema::isValidEntityName(entity_name)) {
        throw MantisException(400, "Invalid Entity name.");
    }

    if (const auto db_type = sess->get_backend_name(); db_type == "sqlite3") {
        *sess << std::format("DROP TRIGGER IF EXISTS mb_{}_insert_trigger",
                             entity_name);
        *sess << std::format("DROP TRIGGER IF EXISTS mb_{}_update_trigger",
                             entity_name);
        *sess << std::format("DROP TRIGGER IF EXISTS mb_{}_delete_trigger",
                             entity_name);
    }
#if MB_HAS_POSTGRESQL
    else if (db_type == "postgresql") {
        *sess << std::format("DROP TRIGGER IF EXISTS mb_{0}_insert_notify ON {0}",
                             entity_name);
        *sess << std::format("DROP TRIGGER IF EXISTS mb_{0}_update_notify ON {0}",
                             entity_name);
        *sess << std::format("DROP TRIGGER IF EXISTS mb_{0}_delete_notify ON {0}",
                             entity_name);
    }
#endif

    else
        throw MantisException(400,
                              std::format("Realtime Mgr: Dropping database hooks not implemented for db type `{}`",
                                          db_type));
}

void mb::RealtimeDB::runWorker(const RtCallback &callback) {
    if (!m_rtDbWorker) {
        m_rtDbWorker = std::make_unique<RtDbWorker>(mbApp());
        m_rtDbWorker->addCallback(callback);
    }
}

void mb::RealtimeDB::stopWorker() const {
    if (m_rtDbWorker) {
        m_rtDbWorker->stopWorker();
    }
}

void mb::RealtimeDB::notifyChange() const {
    // No-op until the worker is started (e.g. during bootstrap or when the
    // server isn't serving). The worker signals itself; this is the push from
    // the write path.
    if (m_rtDbWorker) {
        m_rtDbWorker->notify();
    }
}

#if MB_HAS_POSTGRESQL
void mb::RealtimeDB::createNotifyFunction(const MantisBase& app, soci::session &sql) {
    sql << R"(
            CREATE OR REPLACE FUNCTION mb_notify_changes()
            RETURNS TRIGGER AS $$
            DECLARE
                notification json;
                old_json json;
                new_json json;
            BEGIN
                -- Build row JSON and strip sensitive fields (password)
                IF (TG_OP = 'DELETE') THEN
                    old_json = row_to_json(OLD);
                    old_json = (SELECT json_object_agg(key, value) FROM json_each(old_json) WHERE key != 'password');
                    notification = json_build_object(
                        'timestamp', EXTRACT(EPOCH FROM NOW())::bigint,
                        'type', TG_OP,
                        'entity', TG_TABLE_NAME,
                        'row_id', OLD.id::text,
                        'old_data', old_json,
                        'new_data', NULL
                    );
                ELSE
                    new_json = row_to_json(NEW);
                    new_json = (SELECT json_object_agg(key, value) FROM json_each(new_json) WHERE key != 'password');
                    IF TG_OP = 'UPDATE' THEN
                        old_json = row_to_json(OLD);
                        old_json = (SELECT json_object_agg(key, value) FROM json_each(old_json) WHERE key != 'password');
                    END IF;
                    notification = json_build_object(
                        'timestamp', EXTRACT(EPOCH FROM NOW())::bigint,
                        'type', TG_OP,
                        'entity', TG_TABLE_NAME,
                        'row_id', NEW.id::text,
                        'old_data', CASE WHEN TG_OP = 'UPDATE' THEN old_json ELSE NULL END,
                        'new_data', new_json
                    );
                END IF;

                -- Send notification
                PERFORM pg_notify('mb_db_changes', notification::text);

                IF TG_OP = 'DELETE' THEN
                    RETURN OLD;
                ELSE
                    RETURN NEW;
                END IF;
            END;
            $$ LANGUAGE plpgsql;
        )";

    app.logger().debug("PSQL RTdb",
                   "Created notification function 'mb_notify_changes'");
}
#endif

std::string mb::RealtimeDB::buildTriggerObject(const Entity &entity, const std::string &action) {
    std::stringstream ss;
    ss << "json_object(";
    bool first = true;
    for (const auto &field: entity.fields()) {
        auto field_name = field["name"].get<std::string>();
        if (field_name == "password")
            continue;

        auto name = sqlIdentifier(field_name);
        if (!first)
            ss << ", ";
        ss << "'" << name << "', " << action << "." << name;
        first = false;
    }
    ss << ")";

    return ss.str();
}

mb::RtDbWorker::RtDbWorker(const MantisBase &app)
    : mApp(app), m_running(true) {
    m_db_type = mApp.dbType();
    if (m_db_type == "sqlite3") {
        if (!initSQLite())
            throw MantisException(500, "Worker: SQLite db instantiation failed!");

        mApp.logger().debug("SQLite RTdb",
                       std::format("DB Connection should be active: {}", (isDbRunning() ? "true" : "false")));
    }

#if MB_HAS_POSTGRESQL
    else if (m_db_type == "postgresql") {
        if (!initPSQL())
            throw MantisException(500, "Worker: PostgreSQL db instantiation failed!");

        mApp.logger().debug("PSQL RTdb",
                       std::format("DB Connection should be active: {}", (isDbRunning() ? "true" : "false")));
    }
#endif
    else {
        throw MantisException(500, std::format("Worker: Database type `{}` is not supported!", m_db_type));
    }

    // Setup the thread only after database has been fully initialized
    th = std::thread(&RtDbWorker::run, this);
}

mb::RtDbWorker::~RtDbWorker() {
    stopWorker();
}

bool mb::RtDbWorker::isDbRunning() const {
    if (m_db_type == "sqlite3") return sql_ro && sql_ro->is_connected();
#if MB_HAS_POSTGRESQL
    if (m_db_type == "postgresql")
        return psql && PQstatus(psql.get()) == CONNECTION_OK;
#endif
    return false;
}

void mb::RtDbWorker::addCallback(const RtCallback &cb) {
    m_callback = cb;
}

void mb::RtDbWorker::stopWorker() {
    m_running.store(false);
    cv.notify_all();

    if (th.joinable())
        th.join();

    // Close audit session
    if (sql_ro && sql_ro->is_connected()) {
        sql_ro->close();
    }

#if MB_HAS_POSTGRESQL
    if (psql) {
        // PQfinish(m_pgConn);
        // m_pgConn = nullptr;
        psql.reset();
    }
#endif
}

void mb::RtDbWorker::run() {
    if (!isDbRunning()) {
        throw MantisException(500, "Worker: Database is not running!");
    }

    if (m_db_type == "sqlite3")
        runSQlite();

#if MB_HAS_POSTGRESQL
    else if (m_db_type == "postgresql")
        runPostgreSQL();
#endif

    else
        mApp.logger().critical("RTDb Worker", std::format("Unsupported database type `{}`", m_db_type));
}

void mb::RtDbWorker::runSQlite() {
    // Push-based delivery: sleep until the write path signals a change (see
    // notify()), with a periodic fallback tick as a safety net for writes made
    // outside the CRUD layer (e.g. scripting or migrations). This replaces the
    // old adaptive busy-poll (100ms..5s), so idle load is a single query every
    // few seconds and app-layer writes are delivered with near-zero latency.
    constexpr auto kFallbackInterval = std::chrono::seconds(2);
    constexpr int kBatchSize = 100;
    constexpr int kPruneThreshold = 500;

    while (m_running.load()) {
        {
            std::unique_lock lock(mtx);
            cv.wait_for(lock, kFallbackInterval, [this] {
                return !m_running.load() || m_notified;
            });
            m_notified = false;
        }

        if (!m_running.load()) break;

        // Drain all pending change rows before sleeping again, so a burst of
        // writes is delivered promptly rather than one batch per wake-up.
        try {
            while (m_running.load()) {
                soci::rowset row_set
                        = last_id < 0
                              ? (
                                  sql_ro->prepare <<
                                  "SELECT id, timestamp, type, entity, row_id, old_data, new_data from mb_change_log "
                                  "WHERE timestamp > :ts ORDER BY id ASC LIMIT 100"
                                  , soci::use(last_ts)
                              )
                              : (
                                  sql_ro->prepare <<
                                  "SELECT id, timestamp, type, entity, row_id, old_data, new_data from mb_change_log "
                                  "WHERE id > :last_id ORDER BY id ASC LIMIT 100"
                                  , soci::use(last_id)
                              );

                json res = json::array();

                for (const auto &row: row_set) {
                    auto old_data = row.get_indicator(5) == soci::i_null ? "" : row.get<std::string>(5);
                    auto new_data = row.get_indicator(6) == soci::i_null ? "" : row.get<std::string>(6);

                    auto [od, _0] = tryParseJsonStr(old_data);
                    auto [nd, _1] = tryParseJsonStr(new_data);

                    // TODO: Handle the error gracefully?

                    res.push_back({
                        {"id", row.get<int>(0)},
                        {"timestamp", tmToStr(row.get<std::tm>(1))},
                        {"type", row.get<std::string>(2)},
                        {"entity", row.get<std::string>(3)},
                        {"row_id", row.get<std::string>(4)},
                        {"old_data", od.empty() ? nullptr : od},
                        {"new_data", nd.empty() ? nullptr : nd},
                    });
                }

                if (res.empty()) break; // fully drained

                // Get last element's `id`
                last_id = res.at(res.size() - 1)["id"].get<int>();

                if (m_callback)
                    m_callback(res);

                // Prune consumed rows to keep mb_change_log bounded. This worker
                // is the sole consumer and does not replay history to new
                // subscribers, so rows with id <= last_id have been broadcast
                // and are safe to delete. Batch the deletes.
                if (last_id - m_lastPrunedId >= kPruneThreshold)
                    pruneChangeLog(last_id);

                if (res.size() < static_cast<size_t>(kBatchSize))
                    break; // partial batch => nothing more to read for now
            }
        } catch (std::exception &e) {
            mApp.logger().critical("RTDb Worker", "Realtime Db Worker Error", e.what());
        }
    }

    // Best-effort final prune of everything consumed before the worker exits.
    if (last_id > m_lastPrunedId)
        pruneChangeLog(last_id);
}

void mb::RtDbWorker::notify() {
    {
        std::lock_guard lock(mtx);
        m_notified = true;
    }
    cv.notify_one();
}

void mb::RtDbWorker::pruneChangeLog(const int up_to_id) {
    // The poller connection (sql_ro) is read-only, so acquire a writable
    // session from the main pool for the delete. The pk index on `id` makes
    // this a cheap range delete, and WAL lets it run alongside the poller.
    try {
        const auto write_sql = mApp.db().session();
        *write_sql << "DELETE FROM mb_change_log WHERE id <= :id", soci::use(up_to_id);
        m_lastPrunedId = up_to_id;
    } catch (const std::exception &e) {
        mApp.logger().warn("RTDb Worker", "Change log prune failed", e.what());
    }
}

#if MB_HAS_POSTGRESQL
void mb::RtDbWorker::runPostgreSQL() {
    const auto sleep_for = std::chrono::milliseconds(500);

    while (m_running.load()) {
        {
            std::unique_lock lock(mtx);
            cv.wait_for(lock, sleep_for);
        }

        try {
            // Wait for notifications using select()
            fd_set input_mask;
            FD_ZERO(&input_mask);
            FD_SET(PQsocket(psql.get()), &input_mask);

            struct timeval timeout{};
            timeout.tv_sec = 1; // 1 second timeout
            timeout.tv_usec = 0;

            const int result = select(PQsocket(psql.get()) + 1, &input_mask,
                                      nullptr, nullptr, &timeout);

            if (result < 0) {
                mApp.logger().critical("PSQL RTDb Worker", "PostgreSQL select() failed");
                break;
            }

            if (result == 0) {
                // Timeout - check if we should continue
                continue;
            }

            // Consume input from the connection
            PQconsumeInput(psql.get());

            // Process all available notifications
            PGnotify *notify;
            // TODO add conditional var for disconnecting this inner loop
            while ((notify = PQnotifies(psql.get())) != nullptr) {
                try {
                    // Parse the JSON payload
                    json notification = json::parse(notify->extra);

                    mApp.logger().debug("PSQL RTDb Worker",
                                    std::format("Received notification: {}", notification.dump()));

                    // Call the callback
                    if (m_callback) {
                        json arr = json::array();
                        arr.push_back(notification);
                        m_callback(arr);
                    }
                } catch (const std::exception &e) {
                    mApp.logger().critical("PSQl RTDb Worker",
                                       std::format("Error processing notification: {}", e.what()));
                }

                PQfreemem(notify);
            }

            // Check connection health
            if (PQstatus(psql.get()) != CONNECTION_OK) {
                mApp.logger().warn("PSQl RTDb Worker",
                               "Connection lost, reconnecting...");
                PQfinish(psql.get());
                psql.reset();

                // Try to reconnect
                if (!isDbRunning()) {
                    mApp.logger().critical("PSQl RTDb Worker",
                                       "Reconnection failed, waiting before retry");
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
        } catch (std::exception &e) {
            mApp.logger().critical("PSQl RTDb Worker",
                "Realtime Db Worker Error",
                e.what());
        }
    }
}
#endif

bool mb::RtDbWorker::initSQLite() {
    // Connect to main db
    auto audit_db_path = joinPaths(mApp.dataDir(), "mantis.db").string();

    try {
        // Read-only poller connection. Private cache + WAL lets it read the
        // latest committed snapshot alongside the writable pool; shared_cache is
        // intentionally not enabled (legacy, discouraged with WAL).
        auto sqlite_conn_str = std::format(
            "db={} timeout=30 mode=ro synchronous=normal", audit_db_path);

        sql_ro = std::make_unique<soci::session>(soci::sqlite3, sqlite_conn_str);

        // Enable WAL mode for better concurrency
        *sql_ro << "PRAGMA journal_mode=WAL";
        *sql_ro << "PRAGMA synchronous=NORMAL";
        return true;
    } catch (std::exception &e) {
        mApp.logger().critical(
            "RTDb Worker",
            "Failed to connect to mantis.db database for auditing",
            e.what()
        );
    }

    return false;
}

#if MB_HAS_POSTGRESQL
bool mb::RtDbWorker::initPSQL() {
    const auto &conn_str = mApp.db().connectionStr();
    // Create PSQL object ...
    psql = std::unique_ptr<PGconn, decltype(&PQfinish)>(PQconnectdb(conn_str.c_str()), &PQfinish);

    if (PQstatus(psql.get()) != CONNECTION_OK) {
        mApp.logger().critical("PSQL Notify Worker",
            "Connection failed",
                           std::format("{}", PQerrorMessage(psql.get())));
        // PQfinish(psql.get());
        psql.reset();
        return false;
    }

    // Subscribe to the notification channel
    PGresult *res = PQexec(psql.get(), "LISTEN mb_db_changes");

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        mApp.logger().critical("PSQL Notify Worker",
            "PSQL LISTEN failed",
                           std::format("{}", PQerrorMessage(psql.get())));
        PQclear(res);
        // PQfinish(m_pgConn);
        psql.reset();
        return false;
    }

    PQclear(res);
    mApp.logger().debug("PSQL Notify Worker",
                   "Connected and listening on channel 'mb_db_changes'");
    return true;
}
#endif
