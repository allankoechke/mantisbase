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

        // Overloaded methods with JSON data parameter
        // These take a formatted message string and optional JSON data
        static void trace(const std::string &msg, const json &data = json::object());

        static void info(const std::string &msg, const json &data = json::object());

        static void debug(const std::string &msg, const json &data = json::object());

        static void warn(const std::string &msg, const json &data = json::object());

        static void critical(const std::string &msg, const json &data = json::object());

    private:
        template<typename... Args>
        static void trace(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::trace(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void info(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::info(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void debug(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::debug(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void warn(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::warn(msg, std::forward<Args>(args)...);
        }

        template<typename... Args>
        static void critical(fmt::format_string<Args...> msg, Args &&... args) {
            spdlog::critical(msg, std::forward<Args>(args)...);
        }

        static void logToDatabase(const std::string &level, const std::string &message,
                                  const json &data = json::object());
    };

    using logger = Logger;

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
