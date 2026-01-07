/**
 * @file logging.h
 * @brief Wrapper around spdlog's functionality.
 *
 * Created by allan on 12/05/2025.
 */

#ifndef MANTIS_LOGGER_H
#define MANTIS_LOGGER_H

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include "../../utils/utils.h"

namespace mb {
    using json = nlohmann::json;

    // Forward declaration
    class LogDatabase;

    /**
     * Enum for the different logging levels.
     */
    typedef enum class LogLevel : uint8_t {
        TRACE = 0, ///> Trace logging level
        DEBUG, ///> Debug Logging Level
        INFO, ///> Info Logging Level
        WARN, ///> Warning Logging Level
        CRITICAL ///> Critical Logging Level
    } LogLevel;

    /**
     * A wrapper class around the `spdlog's` logging functions.
     * For more info, check docs here: @see https://github.com/gabime/spdlog
     */
    class Logger {
    public:
        Logger() = default;

        ~Logger() = default;

        inline static bool isDbInitialized = false;

        static void init();

        static void setLogLevel(const LogLevel &level = LogLevel::INFO);

        /**
         * @brief Get the log database instance (for API access).
         * @return Pointer to LogDatabase instance, or nullptr if not initialized
         */
        static LogDatabase &getLogsDb();

        /**
         * @brief Initialize database connection.
         */
        static void initDb(const std::string &data_dir = "");

        // Logging methods with structured format:
        // - origin: Component/system origin (System, Auth, Database, Entity, EntitySchema, etc.)
        // - message: Short message (e.g., "Auth Failed", "Database Connected")
        // - details: Long description of the message
        // - data: Optional JSON object/array with additional data
        static void trace(const std::string &origin, const std::string &message, const std::string &details = "", const json &data = json::object());

        static void info(const std::string &origin, const std::string &message, const std::string &details = "", const json &data = json::object());

        static void debug(const std::string &origin, const std::string &message, const std::string &details = "", const json &data = json::object());

        static void warn(const std::string &origin, const std::string &message, const std::string &details = "", const json &data = json::object());

        static void critical(const std::string &origin, const std::string &message, const std::string &details = "", const json &data = json::object());

    private:
        template<typename... Args>
        static void trace_spdlog(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::trace(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info_spdlog(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::info(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug_spdlog(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::debug(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warn_spdlog(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::warn(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void critical_spdlog(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::critical(msg, std::forward<Args>(args)...);
        }

        static void logToDatabase(const std::string &level, const std::string &origin, const std::string &message, 
                                  const std::string &details, const json &data = json::object());
    };

    using logger = Logger;

    /**
     * @brief Utility logger functions for each component/system.
     * These functions automatically pass the origin parameter.
     */
    namespace LogOrigin {
        // System logging
        inline void trace(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::trace("System", message, details, data);
        }
        inline void info(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::info("System", message, details, data);
        }
        inline void debug(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::debug("System", message, details, data);
        }
        inline void warn(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::warn("System", message, details, data);
        }
        inline void critical(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::critical("System", message, details, data);
        }

        // Auth logging
        inline void authTrace(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::trace("Auth", message, details, data);
        }
        inline void authInfo(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::info("Auth", message, details, data);
        }
        inline void authDebug(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::debug("Auth", message, details, data);
        }
        inline void authWarn(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::warn("Auth", message, details, data);
        }
        inline void authCritical(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::critical("Auth", message, details, data);
        }

        // Database logging
        inline void dbTrace(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::trace("Database", message, details, data);
        }
        inline void dbInfo(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::info("Database", message, details, data);
        }
        inline void dbDebug(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::debug("Database", message, details, data);
        }
        inline void dbWarn(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::warn("Database", message, details, data);
        }
        inline void dbCritical(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::critical("Database", message, details, data);
        }

        // Entity logging
        inline void entityTrace(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::trace("Entity", message, details, data);
        }
        inline void entityInfo(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::info("Entity", message, details, data);
        }
        inline void entityDebug(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::debug("Entity", message, details, data);
        }
        inline void entityWarn(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::warn("Entity", message, details, data);
        }
        inline void entityCritical(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::critical("Entity", message, details, data);
        }

        // EntitySchema logging
        inline void entitySchemaTrace(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::trace("EntitySchema", message, details, data);
        }
        inline void entitySchemaInfo(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::info("EntitySchema", message, details, data);
        }
        inline void entitySchemaDebug(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::debug("EntitySchema", message, details, data);
        }
        inline void entitySchemaWarn(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::warn("EntitySchema", message, details, data);
        }
        inline void entitySchemaCritical(const std::string &message, const std::string &details = "", const json &data = json::object()) {
            Logger::critical("EntitySchema", message, details, data);
        }
    }

    /**
     * @brief A class for tracing function execution [entry, exit]
     * useful in following execution flow
     */
    class FuncLogger {
    public:
        explicit FuncLogger(std::string msg);

        ~FuncLogger();

    private:
        std::string m_msg;
    };
}

// Macro to automatically insert logger with function name
inline std::string getFile(const std::string &path) {
    const std::filesystem::path p = path;
    return p.filename().string();
}

#define MANTIS_FUNC() std::format("{} - {}()", getFile(__FILE__), __FUNCTION__)
#define TRACE_FUNC(x) mb::FuncLogger _logger(x);
#define TRACE_MANTIS_FUNC() try{ mb::FuncLogger _logger(MANTIS_FUNC()); } catch(...){}
#define TRACE_CLASS_METHOD() mb::FuncLogger _logger(std::format("{} {}::{}()", getFile(__FILE__), "", __FUNCTION__));
#define TRACE_METHOD() mb::FuncLogger _logger(std::format("{} {}()", getFile(__FILE__), __FUNCTION__));

#endif //MANTIS_LOGGER_H
