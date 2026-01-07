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
    LogDatabase::LogDatabase() : m_running(false) {
    }

    LogDatabase::~LogDatabase() {
        m_running = false;
        if (m_cleanupThread.joinable()) {
            m_cleanupThread.join();
        }
        if (m_session) {
            m_session->close();
        }
    }

    bool LogDatabase::init(const std::string &data_dir) {
        try {
            m_dataDir = trim(data_dir).empty() ? MantisBase::instance().dataDir() : data_dir;

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
            m_running = true;
            m_cleanupThread = std::thread(&LogDatabase::cleanupThread, this);

            return true;
        } catch (const std::exception &e) {
            // Use spdlog directly to avoid recursion
            spdlog::error("Failed to initialize log database: {}", e.what());
            return false;
        }
    }

    void LogDatabase::createTable() const {
        // Check if table exists and has old schema
        bool needs_migration = false;
        try {
            int count = 0;
            *m_session << "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='mb_logs'", soci::into(count);
            if (count > 0) {
                // Check if old columns exist
                soci::rowset<soci::row> rs = (m_session->prepare << "PRAGMA table_info(mb_logs)");
                bool has_topic = false;
                bool has_origin = false;
                for (const auto& r : rs) {
                    std::string col_name = r.get<std::string>(1);
                    if (col_name == "topic") has_topic = true;
                    if (col_name == "origin") has_origin = true;
                }
                if (has_topic && !has_origin) {
                    needs_migration = true;
                }
            }
        } catch (...) {
            // Table might not exist, continue with creation
        }

        if (needs_migration) {
            // Migrate old schema to new schema
            *m_session << R"(
                CREATE TABLE IF NOT EXISTS mb_logs_new (
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
            
            // Copy data from old table to new (mapping topic->message, description->details, adding origin)
            *m_session << R"(
                INSERT INTO mb_logs_new (id, timestamp, level, origin, message, details, data, created_at)
                SELECT id, timestamp, level, 
                       COALESCE(topic, 'System') as origin,
                       COALESCE(topic, 'Unknown') as message,
                       COALESCE(description, '') as details,
                       COALESCE(data, '') as data,
                       created_at
                FROM mb_logs
            )";
            
            // Drop old table and rename new one
            *m_session << "DROP TABLE mb_logs";
            *m_session << "ALTER TABLE mb_logs_new RENAME TO mb_logs";
        } else {
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
        }

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
            // Get current timestamp
            const auto now = std::chrono::system_clock::now();
            const auto time_t = std::chrono::system_clock::to_time_t(now);
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now.time_since_epoch()) % 1000;

            // Format timestamp as ISO 8601
            std::stringstream ss;
            ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
            ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
            std::string timestamp = ss.str();

            // Get Unix timestamp for sorting/cleanup
            auto created_at = std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();


            { // Local scope
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

    json LogDatabase::getLogs(int page, int page_size,
                              const std::string &level_filter,
                              const std::string &search_filter,
                              const std::string &start_date,
                              const std::string &end_date,
                              const std::string &sort_by,
                              const std::string &sort_order) {
        json result = json::object();
        json logs_array = json::array();

        try {
            // Build WHERE clause
            std::vector<std::string> conditions;

            if (!level_filter.empty()) {
                conditions.emplace_back("level = :level");
            }
            if (!search_filter.empty()) {
                conditions.emplace_back("(message LIKE :search OR details LIKE :search)");
            }
            if (!start_date.empty()) {
                conditions.emplace_back("timestamp >= :start_date");
            }
            if (!end_date.empty()) {
                conditions.emplace_back("timestamp <= :end_date");
            }

            // if (!conditions.empty()) {
            //     std::string where_clause;
            //     where_clause = "WHERE ";
            //     for (size_t i = 0; i < conditions.size(); ++i) {
            //         if (i > 0) where_clause += " AND ";
            //         where_clause += conditions[i];
            //     }
            // }

            // Validate sort_by to prevent SQL injection
            std::string valid_sort_by = "timestamp";
            if (sort_by == "level" || sort_by == "origin" || sort_by == "message" || sort_by == "details" || sort_by == "timestamp" || sort_by == "created_at") {
                valid_sort_by = sort_by;
            }

            // Validate sort_order
            std::string valid_sort_order = (sort_order == "asc") ? "ASC" : "DESC";

            // Calculate offset
            int offset = (page - 1) * page_size;

            // Build query with proper WHERE clause (inputs validated, safe to use)
            std::string query = "SELECT id, timestamp, level, origin, message, details, data, created_at FROM mb_logs";

            // Build WHERE clause with actual values (after validation, safe)
            if (!conditions.empty()) {
                query += " WHERE ";
                bool first = true;
                if (!level_filter.empty()) {
                    // level_filter is validated to be one of: trace, debug, info, warn, critical
                    query += (first ? "" : " AND ") + std::string("level = '") + level_filter + "'";
                    first = false;
                }
                if (!search_filter.empty()) {
                    // Escape single quotes for SQL
                    std::string escaped = search_filter;
                    size_t pos = 0;
                    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
                        escaped.replace(pos, 1, "''");
                        pos += 2;
                    }
                    query += (first ? "" : " AND ") + std::string("(message LIKE '%") + escaped + "%' OR details LIKE '%" + escaped + "%')";
                    first = false;
                }
                if (!start_date.empty()) {
                    // start_date is validated format
                    std::string escaped = start_date;
                    size_t pos = 0;
                    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
                        escaped.replace(pos, 1, "''");
                        pos += 2;
                    }
                    query += (first ? "" : " AND ") + std::string("timestamp >= '") + escaped + "'";
                    first = false;
                }
                if (!end_date.empty()) {
                    std::string escaped = end_date;
                    size_t pos = 0;
                    while ((pos = escaped.find('\'', pos)) != std::string::npos) {
                        escaped.replace(pos, 1, "''");
                        pos += 2;
                    }
                    query += (first ? "" : " AND ") + std::string("timestamp <= '") + escaped + "'";
                    first = false;
                }
            }

            query += " ORDER BY " + valid_sort_by + " " + valid_sort_order;
            query += " LIMIT " + std::to_string(page_size) + " OFFSET " + std::to_string(offset);

            // Acquire lock
            std::lock_guard lock(m_dbMutexLock);

            // Execute query using rowset and iterate over results
            for (const soci::rowset rs = (m_session->prepare << query); const auto &r: rs) {
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

            // Get total count
            int total_count = -1; // getLogCount(level_filter, search_filter, start_date, end_date);

            result["data"] = json::object();
            result["data"]["page"] = page;
            result["data"]["page_size"] = page_size;
            result["data"]["total_count"] = total_count;
            result["data"]["items"] = logs_array;
        } catch (const std::exception &e) {
            throw MantisException(500, std::string("Failed to fetch logs: ") + e.what());
        }

        return result;
    }

    int LogDatabase::getLogCount(const std::string &level_filter,
                                 const std::string &search_filter,
                                 const std::string &start_date,
                                 const std::string &end_date) {
        try {
            // Build WHERE clause (same as getLogs)
            std::vector<std::string> conditions;

            if (!level_filter.empty()) {
                conditions.emplace_back("level = :level");
            }
            if (!search_filter.empty()) {
                conditions.emplace_back("(message LIKE :search OR details LIKE :search)");
            }
            if (!start_date.empty()) {
                conditions.emplace_back("timestamp >= :start_date");
            }
            if (!end_date.empty()) {
                conditions.emplace_back("timestamp <= :end_date");
            }

            // TODO check on where clause??
            // if (!conditions.empty()) {
            //     std::string where_clause;
            //     where_clause = "WHERE ";
            //     for (size_t i = 0; i < conditions.size(); ++i) {
            //         if (i > 0) where_clause += " AND ";
            //         where_clause += conditions[i];
            //     }
            // }

            // Build COUNT query with WHERE clause (inputs validated, safe)
            std::string query = "SELECT COUNT(*) FROM mb_logs";

            if (!conditions.empty()) {
                query += " WHERE ";
                bool first = true;
                if (!level_filter.empty()) {
                    query += (first ? "" : " AND ") + std::string("level = '") + level_filter + "'";
                    first = false;
                }
                if (!search_filter.empty()) {
                    std::string escaped = search_filter;
                    size_t pos = 0;
                    while ((pos = escaped.find("'", pos)) != std::string::npos) {
                        escaped.replace(pos, 1, "''");
                        pos += 2;
                    }
                    query += (first ? "" : " AND ") + std::string("(message LIKE '%") + escaped + "%' OR details LIKE '%" + escaped + "%')";
                    first = false;
                }
                if (!start_date.empty()) {
                    std::string escaped = start_date;
                    size_t pos = 0;
                    while ((pos = escaped.find("'", pos)) != std::string::npos) {
                        escaped.replace(pos, 1, "''");
                        pos += 2;
                    }
                    query += (first ? "" : " AND ") + std::string("timestamp >= '") + escaped + "'";
                    first = false;
                }
                if (!end_date.empty()) {
                    std::string escaped = end_date;
                    size_t pos = 0;
                    while ((pos = escaped.find("'", pos)) != std::string::npos) {
                        escaped.replace(pos, 1, "''");
                        pos += 2;
                    }
                    query += (first ? "" : " AND ") + std::string("timestamp <= '") + escaped + "'";
                    first = false;
                }
            }

            int count = 0;
            soci::row r;

            std::lock_guard<std::mutex> lock(m_dbMutexLock);
            *m_session << query, soci::into(count);

            return count;
        } catch (const std::exception &e) {
            return 0;
        }
    }

    void LogDatabase::cleanupThread() {
        // Run cleanup every hour
        const auto cleanup_interval = std::chrono::hours(1);

        while (m_running) {
            std::this_thread::sleep_for(cleanup_interval);
            if (m_running) {
                deleteOldLogs(5);
            }
        }
    }

    void LogDatabase::deleteOldLogs(const int days) {
        try {
            std::lock_guard<std::mutex> lock(m_dbMutexLock);

            // Calculate cutoff timestamp (5 days ago)
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
}
