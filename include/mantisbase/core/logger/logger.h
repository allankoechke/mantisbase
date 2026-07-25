/**
 * @file logging.h
 * @brief Wrapper around spdlog's functionality.
 *
 * Created by allan on 12/05/2025.
 */

#ifndef MB_LOGGER_H
#define MB_LOGGER_H

#include <iostream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>

#include "log_database.h"
#include "../../utils/utils.h"
#include "../../core/types.h"

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
        std::unique_ptr<LogDatabase> m_logsDb;
        const MantisBase &mApp;

    public:
        explicit Logger(const MantisBase &app);

        static void setLogLevel(const LogLevel &level = LogLevel::INFO);

        /**
         * @brief Get the log database instance (for API access).
         * @return Pointer to LogDatabase instance, or nullptr if not initialized
         */
        [[nodiscard]] LogDatabase &logsDb() const;

        /**
         * @brief Initialize database connection.
         */
        void initDb(const std::string &data_dir = "") const;

        // Logging methods with structured format:
        // - origin: Component/system origin (System, Auth, Database, Entity, EntitySchema, etc.)
        // - message: Short message (e.g., "Auth Failed", "Database Connected")
        // - details: Long description of the message
        // - data: Optional JSON object/array with additional data
        void trace(const std::string &origin,
                   const std::string &message,
                   const std::string &details = "",
                   const json &data = json::object()) const;

        void info(const std::string &origin,
                  const std::string &message,
                  const std::string &details = "",
                  const json &data = json::object()) const;

        void debug(const std::string &origin,
                   const std::string &message,
                   const std::string &details = "",
                   const json &data = json::object()) const;

        void warn(const std::string &origin,
                  const std::string &message,
                  const std::string &details = "",
                  const json &data = json::object()) const;

        void critical(const std::string &origin,
                      const std::string &message,
                      const std::string &details = "",
                      const json &data = json::object()) const;


        inline static std::atomic<bool> isDbInitialized = false;

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

        void logToDatabase(const std::string &level,
                           const std::string &origin,
                           const std::string &message,
                           const std::string &details,
                           const json &data = json::object()) const;
    };

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

#define MB_FUNC() std::format("{} - {}()", getFile(__FILE__), __FUNCTION__)
#define TRACE_FUNC(x) mb::FuncLogger _logger(x);
#define TRACE_MB_FUNC() try{ mb::FuncLogger _logger(MB_FUNC()); } catch(...){}
#define TRACE_CLASS_METHOD() mb::FuncLogger _logger(std::format("{} {}::{}()", getFile(__FILE__), "", __FUNCTION__));
#define TRACE_METHOD() mb::FuncLogger _logger(std::format("{} {}()", getFile(__FILE__), __FUNCTION__));

#endif // MB_LOGGER_H
