#include "../../include/mantisbase/core/realtime.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/exceptions.h"
#include "../../include/mantisbase/core/database.h"

#include <soci/soci.h>
#include "soci/sqlite3/soci-sqlite3.h"

mb::RealtimeDB::RealtimeDB()
    : mApp(mb::MantisBase::instance()) {
}

bool mb::RealtimeDB::init() const {
    try {
        const auto &sql = mApp.db().session();

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

#if MANTIS_HAS_POSTGRESQL
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

#if MANTIS_HAS_POSTGRESQL
        // For PostgreSQL, create the hook function
        if (sql->get_backend_name() == "postgresql") {
            createNotifyFunction(*sql);
        }
#endif

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

#if MANTIS_HAS_POSTGRESQL
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
    const auto sql = mApp.db().session();
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
#if MANTIS_HAS_POSTGRESQL
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
        m_rtDbWorker = std::make_unique<RtDbWorker>();
        m_rtDbWorker->addCallback(callback);
    }
}

void mb::RealtimeDB::stopWorker() const {
    if (m_rtDbWorker) {
        m_rtDbWorker->stopWorker();
    }
}

#if MANTIS_HAS_POSTGRESQL
void mb::RealtimeDB::createNotifyFunction(soci::session &sql) {
    sql << R"(
            CREATE OR REPLACE FUNCTION mb_notify_changes()
            RETURNS TRIGGER AS $$
            DECLARE
                notification json;
            BEGIN
                -- Build notification payload
                IF (TG_OP = 'DELETE') THEN
                    notification = json_build_object(
                        'timestamp', EXTRACT(EPOCH FROM NOW())::bigint,
                        'type', TG_OP,
                        'entity', TG_TABLE_NAME,
                        'row_id', OLD.id::text,
                        'old_data', row_to_json(OLD),
                        'new_data', NULL
                    );
                ELSE
                    notification = json_build_object(
                        'timestamp', EXTRACT(EPOCH FROM NOW())::bigint,
                        'type', TG_OP,
                        'entity', TG_TABLE_NAME,
                        'row_id', NEW.id::text,
                        'old_data', CASE WHEN TG_OP = 'UPDATE' THEN row_to_json(OLD) ELSE NULL END,
                        'new_data', row_to_json(NEW)
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

    logEntry::info("PostgreSQL Realtime",
                   "Created notification function 'mb_notify_changes'");
}
#endif

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
    : m_running(true) {
    m_db_type = MantisBase::instance().dbType();
    if (m_db_type == "sqlite3") {
        if (!initSQLite())
            throw MantisException(500, "Worker: SQLite db instantiation failed!");

        logEntry::info("RTDb Worker",
                       "SQLite Database Status",
                       std::format("DB Connection should be active: {}", (isDbRunning() ? "true" : "false")));
    }

#if MANTIS_HAS_POSTGRESQL
    else if (m_db_type == "postgresql") {
        if (!initPSQL())
            throw MantisException(500, "Worker: PostgreSQL db instantiation failed!");

        logEntry::info("RTDb Worker",
                       "PSQL Database Status",
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
#if MANTIS_HAS_POSTGRESQL
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

#if MANTIS_HAS_POSTGRESQL
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

#if MANTIS_HAS_POSTGRESQL
    else if (m_db_type == "postgresql")
        runPostgreSQL();
#endif

    else
        logEntry::critical("RTDb Worker", "Unsupported database",
                           std::format("Unsupported database type `{}`", m_db_type));
}

void mb::RtDbWorker::runSQlite() {
    int emptyPollCount = 0;
    auto sleep_for = std::chrono::milliseconds(500);

    while (m_running.load()) {
        {
            std::unique_lock lock(mtx);
            cv.wait_for(lock, sleep_for);
        }

        try {
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

                auto od = tryParseJsonStr(old_data, json::object()).value();
                auto nd = tryParseJsonStr(new_data, json::object()).value();

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

            if (!res.empty()) {
                // Get last element's `id`
                last_id = res.at(res.size() - 1)["id"].get<int>();

                emptyPollCount = 0;
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
            logEntry::critical("RTDb Worker", "Realtime Db Worker Error", e.what());
        }
    }
}

#if MANTIS_HAS_POSTGRESQL
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

            struct timeval timeout;
            timeout.tv_sec = 1; // 1 second timeout
            timeout.tv_usec = 0;

            const int result = select(PQsocket(psql.get()) + 1, &input_mask,
                                      nullptr, nullptr, &timeout);

            if (result < 0) {
                logEntry::critical("[PSQl] RTDb Worker", "PostgreSQL Notify Worker", "select() failed");
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

                    logEntry::debug("[PSQl] RTDb Worker", "PostgreSQL Notify Worker",
                                    std::format("Received notification: {}", notification.dump()));

                    // Call the callback
                    if (m_callback) {
                        json arr = json::array();
                        arr.push_back(notification);
                        m_callback(arr);
                    }
                } catch (const std::exception &e) {
                    logEntry::critical("[PSQl] RTDb Worker", "PostgreSQL Notify Worker",
                                       std::format("Error processing notification: {}", e.what()));
                }

                PQfreemem(notify);
            }

            // Check connection health
            if (PQstatus(psql.get()) != CONNECTION_OK) {
                logEntry::warn("[PSQl] RTDb Worker", "PostgreSQL Notify Worker",
                               "Connection lost, reconnecting...");
                PQfinish(psql.get());
                psql.reset();

                // Try to reconnect
                if (!isDbRunning()) {
                    logEntry::critical("[PSQl] RTDb Worker", "PostgreSQL Notify Worker",
                                       "Reconnection failed, waiting before retry");
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
        } catch (std::exception &e) {
            logEntry::critical("[PSQl] RTDb Worker", "Realtime Db Worker Error", e.what());
        }
    }
}
#endif

bool mb::RtDbWorker::initSQLite() {
    // Connect to main db
    auto audit_db_path = joinPaths(MantisBase::instance().dataDir(), "mantis.db").string();

    try {
        auto sqlite_conn_str = std::format(
            "db={} timeout=30 mode=ro shared_cache=true synchronous=normal", audit_db_path);

        sql_ro = std::make_unique<soci::session>(soci::sqlite3, sqlite_conn_str);

        // Enable WAL mode for better concurrency
        *sql_ro << "PRAGMA journal_mode=WAL";
        *sql_ro << "PRAGMA synchronous=NORMAL";
        return true;
    } catch (std::exception &e) {
        logEntry::critical(
            "Realtime Db Worker",
            "Failed to connect to mantis.db database for auditing",
            e.what()
        );
    }

    return false;
}

#if MANTIS_HAS_POSTGRESQL
bool mb::RtDbWorker::initPSQL() {
    const auto &conn_str = MantisBase::instance().db().connectionStr();
    // Create PSQL object ...
    psql = std::unique_ptr<PGconn, decltype(&PQfinish)>(PQconnectdb(conn_str.c_str()), &PQfinish);

    if (PQstatus(psql.get()) != CONNECTION_OK) {
        logEntry::critical("PostgreSQL Notify Worker",
                           std::format("Connection failed: {}", PQerrorMessage(psql.get())));
        // PQfinish(psql.get());
        psql.reset();
        return false;
    }

    // Subscribe to the notification channel
    PGresult *res = PQexec(psql.get(), "LISTEN mb_db_changes");

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        logEntry::critical("PostgreSQL Notify Worker",
                           std::format("LISTEN failed: {}", PQerrorMessage(psql.get())));
        PQclear(res);
        // PQfinish(m_pgConn);
        psql.reset();
        return false;
    }

    PQclear(res);
    logEntry::info("PostgreSQL Notify Worker",
                   "Connected and listening on channel 'mb_db_changes'");
    return true;
}
#endif
