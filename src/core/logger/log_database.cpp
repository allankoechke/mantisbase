#include "../../../include/mantisbase/core/logger/log_database.h"
#include "../../../include/mantisbase/core/logger/logger.h"
#include "../../../include/mantisbase/utils/utils.h"

#include <soci/sqlite3/soci-sqlite3.h>
#include <soci/soci.h>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "mantisbase/mantisbase.h"

namespace mb {
    LogDatabase::LogDatabase(const MantisBase &app) : m_running(false), mApp(app) {
    }

    LogDatabase::~LogDatabase() {
        if (m_running.load()) {
            Logger::isDbInitialized.store(false);
            shutdown();
        }
    }

    bool LogDatabase::init(const std::string &data_dir) {
        try {
            m_dataDir = trim(data_dir).empty() ? mApp.dataDir() : data_dir;

            // Create log database path
            auto log_db_path = joinPaths(m_dataDir, "mantis_logs.db").string();

            // Open SQLite connection
            m_session = std::make_unique<soci::session>(soci::sqlite3,
                                                        std::format("db={} timeout=30", log_db_path));

            // Enable WAL mode for better concurrency
            *m_session << "PRAGMA journal_mode=WAL";
            *m_session << "PRAGMA synchronous=normal";

            // Create table
            createTable();

            // Start cleanup thread
            m_running.store(true);
            m_cleanupThread = std::thread(&LogDatabase::cleanupThread, this);

            return true;
        } catch (const std::exception &e) {
            // Use spdlog directly to avoid recursion
            spdlog::error("Failed to initialize log database: {}", e.what());
            return false;
        }
    }

    void LogDatabase::shutdown() {
        m_running.store(false);
        m_cv.notify_all();

        if (m_cleanupThread.joinable()) m_cleanupThread.join();
        if (m_session) {
            m_session->close();
            m_session.reset();
        }
    }

    void LogDatabase::createTable() const {
        if (!m_session) {
            std::cerr << "LogDatabase::createTable: Session is NULL" << std::endl;
            return;
        }

        // Create new table schema
        *m_session << R"(
                CREATE TABLE IF NOT EXISTS mb_logs (
                    id text PRIMARY KEY,
                    timestamp TEXT NOT NULL,
                    level TEXT NOT NULL,
                    origin TEXT NOT NULL,
                    message TEXT NOT NULL,
                    details TEXT,
                    data TEXT,
                    created_at INTEGER NOT NULL
                )
            )";

        // Create indexes for better query performance
        *m_session << "CREATE INDEX IF NOT EXISTS idx_mb_logs_timestamp ON mb_logs(timestamp)";
        *m_session << "CREATE INDEX IF NOT EXISTS idx_mb_logs_level ON mb_logs(level)";
        *m_session << "CREATE INDEX IF NOT EXISTS idx_mb_logs_origin ON mb_logs(origin)";
        *m_session << "CREATE INDEX IF NOT EXISTS idx_mb_logs_message ON mb_logs(message)";
        *m_session << "CREATE INDEX IF NOT EXISTS idx_mb_logs_created_at ON mb_logs(created_at)";
    }

    bool LogDatabase::insertLog(const std::string &level, const std::string &origin, const std::string &message,
                                const std::string &details, const json &data) {
        try {
            if (!m_session) {
                std::cerr << "LogDatabase::insertLog: Insert error, session is NULL" << std::endl;
                return false;
            }

            // Get current timestamp
            const auto now = std::chrono::system_clock::now();
            const auto time_t = std::chrono::system_clock::to_time_t(now);
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now.time_since_epoch()) % 1000;

            // Format timestamp as ISO 8601 (UTC)
            const std::tm utc = toUtcTime(time_t);
            std::stringstream ss;
            ss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
            ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";

            {
                std::string timestamp = ss.str();

                // Get Unix timestamp for sorting/cleanup
                auto created_at = std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count();

                // Local scope
                // Serialize data JSON
                std::string data_str = data.empty() ? "" : data.dump();
                auto id = generate_uuidv7();

                std::lock_guard lock(m_dbMutexLock);

                // Insert log entry
                *m_session << "INSERT INTO mb_logs (id, timestamp, level, origin, message, details, data, created_at) "
                        "VALUES (:id, :timestamp, :level, :origin, :message, :details, :data, :created_at)",
                        soci::use(id), soci::use(timestamp), soci::use(level), soci::use(origin),
                        soci::use(message), soci::use(details), soci::use(data_str), soci::use(created_at);
            }

            return true;
        } catch (const std::exception &e) {
            // Use spdlog directly to avoid recursion
            spdlog::error("Failed to insert log: {}", e.what());
            return false;
        }
    }

    json LogDatabase::getLogs(const std::string &after, int limit,
                              const std::string &level_filter,
                              const std::string &search_filter,
                              const std::string &start_date,
                              const std::string &end_date) {
        if (!m_session) {
            std::cerr << "LogDatabase::getLogs: Fetch error, session is NULL" << std::endl;
            return json::object();
        }

        if (limit < 1) limit = 1;
        if (limit > 1000) limit = 1000;

        const int fetch_limit = limit + 1;
        json result = json::object();
        json logs_array = json::array();

        try {
            std::string query = "SELECT id, timestamp, level, origin, message, details, data, created_at FROM mb_logs";

            std::vector<std::string> conditions;

            std::string search_like;

            soci::statement st(*m_session);
            soci::row r;
            st.exchange(soci::into(r));

            if (!after.empty()) {
                conditions.emplace_back("id < :after");
                st.exchange(soci::use(after, "after"));
            }

            if (!level_filter.empty()) {
                if (level_filter.starts_with('>')) {
                    conditions.push_back(buildMinLogWhereCondition(level_filter.substr(1)));
                } else {
                    conditions.push_back("level = :level_filter");
                    st.exchange(soci::use(level_filter, "level_filter"));
                }
            }

            if (!search_filter.empty()) {
                search_like = "%" + search_filter + "%";
                conditions.push_back("(message LIKE :search_filter OR details LIKE :search_filter)");
                st.exchange(soci::use(search_like, "search_filter"));
            }

            if (!start_date.empty()) {
                conditions.push_back("timestamp >= :start_date");
                st.exchange(soci::use(start_date, "start_date"));
            }
            if (!end_date.empty()) {
                conditions.push_back("timestamp <= :end_date");
                st.exchange(soci::use(end_date, "end_date"));
            }

            if (!conditions.empty()) {
                query += " WHERE ";
                for (size_t i = 0; i < conditions.size(); ++i) {
                    if (i > 0) query += " AND ";
                    query += conditions[i];
                }
            }

            query += " ORDER BY id DESC LIMIT :limit";

            std::lock_guard lock(m_dbMutexLock);

            st.exchange(soci::use(fetch_limit, "limit"));
            st.alloc();
            st.prepare(query);
            st.define_and_bind();
            st.execute();

            while (st.fetch()) {
                json log_entry = json::object();
                log_entry["id"] = r.get<std::string>(0);
                log_entry["timestamp"] = r.get<std::string>(1);
                log_entry["level"] = r.get<std::string>(2);
                log_entry["origin"] = r.get<std::string>(3);
                log_entry["message"] = r.get<std::string>(4);
                log_entry["details"] = r.get<std::string>(5);
                log_entry["data"] = json::object();

                if (auto data_str = r.get<std::string>(6); !trim(data_str).empty()) {
                    try {
                        log_entry["data"] = json::parse(data_str);
                    } catch (...) {
                        log_entry["data"] = trim(data_str);
                    }
                }

                log_entry["created_at"] = r.get<long long>(7);
                logs_array.push_back(log_entry);
            }

            bool has_more = false;
            if (static_cast<int>(logs_array.size()) > limit) {
                has_more = true;
                logs_array.erase(logs_array.begin() + limit, logs_array.end());
            }

            std::string cursor;
            if (!logs_array.empty()) {
                const auto &last = logs_array.back();
                if (last.contains("id"))
                    cursor = last["id"].get<std::string>();
            }

            result["data"] = json::object();
            result["data"]["limit"] = limit;
            result["data"]["has_more"] = has_more;
            result["data"]["cursor"] = cursor;
            result["data"]["items"] = logs_array;
            result["data"]["items_count"] = logs_array.size();
        } catch (const std::exception &e) {
            throw MantisException(500, std::string("Failed to fetch logs: ") + e.what());
        }

        return result;
    }

    void LogDatabase::cleanupThread() {
        // Run cleanup every hour
        const auto cleanup_interval = std::chrono::hours(1);

        while (m_running.load()) {
            {
                std::unique_lock<std::mutex> lock(m_dbMutexLock);
                m_cv.wait_for(lock, std::chrono::milliseconds(cleanup_interval));
            }

            deleteOldLogs(configuredLogRetentionDays());
        }
    }

    int LogDatabase::configuredLogRetentionDays() const {
        constexpr int kDefaultLogRetentionDays = 5;

        try {
            const auto &cfg = mApp.settings().configs();
            if (cfg.contains("logRetentionDays") && cfg["logRetentionDays"].is_number_integer()) {
                const int days = cfg["logRetentionDays"].get<int>();
                if (days > 0) {
                    return days;
                }
            }
        } catch (...) {
        }

        return kDefaultLogRetentionDays;
    }

    void LogDatabase::deleteOldLogs(const int days) {
        try {
            if (!m_session) {
                std::cerr << "LogDatabase::deleteOldLogs: Delete error, session is NULL" << std::endl;
                return;
            }

            std::lock_guard lock(m_dbMutexLock);

            // Calculate cutoff timestamp
            const auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * days);
            auto cutoff_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                cutoff.time_since_epoch()).count();

            // Delete old logs
            *m_session << "DELETE FROM mb_logs WHERE created_at < :cutoff",
                    soci::use(cutoff_seconds);

            // Vacuum database periodically to reclaim space
            static int vacuum_counter = 0;
            if (++vacuum_counter % 24 == 0) {
                // Every 24 hours (once per day)
                *m_session << "VACUUM";
            }
        } catch (const std::exception &e) {
            // Use spdlog directly to avoid recursion
            spdlog::error("Failed to delete old logs: {}", e.what());
        }
    }

    std::string LogDatabase::buildMinLogWhereCondition(const std::string &level) {
        std::stringstream ss;
        bool first = true;

        ss << "(";

        for (const auto &l: m_logLevels) {
            if (first) {
                ss << std::format("level = '{}'", l);
                first = false;
                if (l == level) break;
                continue;
            }

            ss << std::format(" OR level = '{}'", l);
            if (l == level) break;
        }

        ss << ")";

        return ss.str();
    }
}
