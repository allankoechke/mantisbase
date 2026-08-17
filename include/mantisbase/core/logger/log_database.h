/**
 * @file log_database.h
 * @brief SQLite database manager for application logs.
 *
 * Manages a separate SQLite database for storing application logs with
 * automatic cleanup of old records.
 */

#ifndef MB_LOG_DATABASE_H
#define MB_LOG_DATABASE_H

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>

namespace soci {
    class session;
}

namespace mb {
    class MantisBase;
    using json = nlohmann::json;

    /**
     * @brief Manages SQLite database for application logs.
     *
     * Provides methods to store logs in a separate SQLite database,
     * with automatic cleanup of logs older than the configured retention period.
     */
    class LogDatabase {
    public:
        /**
         * @brief Construct LogDatabase instance.
         */
        explicit LogDatabase(const MantisBase& app);

        /**
         * @brief Destructor - stops cleanup thread and closes database.
         */
        ~LogDatabase();

        /**
         * @brief Initialize database: create table and start cleanup thread.
         * @return true if initialization successful, false otherwise
         */
        bool init(const std::string& data_dir = "");

        /**
         * @brief Insert a log entry into the database.
         * @param level Log level (trace, debug, info, warn, critical)
         * @param origin Component/system origin (System, Auth, Database, Entity, EntitySchema, etc.)
         * @param message Short message (e.g., "Auth Failed", "Database Connected")
         * @param details Long description of the message
         * @param data Optional JSON data associated with the log
         * @return true if insert successful, false otherwise
         */
        bool insertLog(const std::string& level, const std::string& origin, const std::string& message, 
                       const std::string& details, const json& data = json::object());

        /**
         * @brief Get logs with cursor-based pagination and filtering (ordered by `id` ASC).
         * @param after Cursor ID to fetch records after (empty = start from beginning)
         * @param limit Maximum number of records to return (default 50, max 1000)
         * @param level_filter Optional level filter (empty = all levels)
         * @param search_filter Optional message search filter (empty = no filter)
         * @param start_date Optional start date filter (ISO 8601 format, empty = no filter)
         * @param end_date Optional end date filter (ISO 8601 format, empty = no filter)
         * @return JSON object with logs, cursor, and has_more
         */
        json getLogs(const std::string& after = "", int limit = 50,
                     const std::string& level_filter = "",
                     const std::string& search_filter = "",
                     const std::string& start_date = "",
                     const std::string& end_date = "");
    private:
        /**
         * @brief Shutdown and clean up log database.
         */
        void shutdown();

        /**
         * @brief Create the logs table if it doesn't exist.
         */
        void createTable() const;

        /**
         * @brief Background thread function to delete old logs.
         */
        void cleanupThread();

        /**
         * @brief Delete logs older than specified days.
         * @param days Number of days to keep
         */
        void deleteOldLogs(int days);

        [[nodiscard]] int configuredLogRetentionDays() const;

        static std::string buildMinLogWhereCondition(const std::string& level);

        std::string m_dataDir;
        std::unique_ptr<soci::session> m_session;
        std::thread m_cleanupThread;
        std::atomic<bool> m_running;
        std::condition_variable m_cv;
        std::mutex m_dbMutexLock;
        const MantisBase& mApp;

        inline static const std::vector<std::string> m_logLevels{ "critical", "warn", "info", "debug", "trace"};
    };
}

#endif // MB_LOG_DATABASE_H

