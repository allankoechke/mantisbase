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
#include "../utils/utils.h"

namespace mb
{
    using json = nlohmann::json;

    /**
     * Enum for the different logging levels.
     */
    typedef enum class LogLevel : uint8_t
    {
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
    class LogsMgr
    {
    public:
        LogsMgr() = default;
        ~LogsMgr() = default;

        static void init();
        static void setLogLevel(const LogLevel& level = LogLevel::INFO);

        template <typename... Args>
        static void trace(fmt::format_string<Args...> msg, Args&&... args)
        {
            spdlog::trace(msg, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void info(fmt::format_string<Args...> msg, Args&&... args)
        {
            spdlog::info(msg, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void debug(fmt::format_string<Args...> msg, Args&&... args)
        {
            spdlog::debug(msg, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void warn(fmt::format_string<Args...> msg, Args&&... args)
        {
            spdlog::warn(msg, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void critical(fmt::format_string<Args...> msg, Args&&... args)
        {
            spdlog::critical(msg, std::forward<Args>(args)...);
        }
    };

    using logger = LogsMgr;

    /**
     * @brief A class for tracing function execution [entry, exit]
     * useful in following execution flow
     */
    class FuncLogger
    {
    public:
        explicit FuncLogger(const std::string& msg);

        ~FuncLogger();

    private:
        std::string m_msg;
    };
}

// Macro to automatically insert logger with function name
inline std::string getFile(const std::string& path) {
    const std::filesystem::path p = path;
    return p.filename().string();
}

#define MANTIS_FUNC() std::format("{} - {}()", getFile(__FILE__), __FUNCTION__)
#define TRACE_FUNC(x) mb::FuncLogger _logger(x);
#define TRACE_MANTIS_FUNC() try{ mb::FuncLogger _logger(MANTIS_FUNC()); } catch(...){}
#define TRACE_CLASS_METHOD() mb::FuncLogger _logger(std::format("{} {}::{}()", getFile(__FILE__), "", __FUNCTION__));
#define TRACE_METHOD() mb::FuncLogger _logger(std::format("{} {}()", getFile(__FILE__), __FUNCTION__));

#endif //MANTIS_LOGGER_H
