#include "../../../include/mantisbase/core/logger/logger.h"
#include "../../../include/mantisbase/core/logger/log_database.h"
#include <spdlog/sinks/stdout_color_sinks-inl.h>

#include <utility>

#include "mantisbase/mantisbase.h"
#include "spdlog/sinks/ansicolor_sink.h"

void mb::Logger::setLogLevel(const LogLevel &level) {
    const auto set_spdlog_level = [&](const spdlog::level::level_enum lvl) {
        const auto logger = spdlog::get("mantis_logger");
        logger->set_level(lvl);

        // Also set sink levels
        for (const auto &sink: logger->sinks()) {
            sink->set_level(lvl);
        }
    };

    switch (level) {
        case LogLevel::TRACE:
            set_spdlog_level(spdlog::level::trace);
            break;
        case LogLevel::DEBUG:
            set_spdlog_level(spdlog::level::debug);
            break;
        case LogLevel::INFO:
            set_spdlog_level(spdlog::level::info);
            break;
        case LogLevel::WARN:
            set_spdlog_level(spdlog::level::warn);
            break;
        case LogLevel::CRITICAL:
            set_spdlog_level(spdlog::level::critical);
            break;
    }
}

void mb::Logger::init() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%-8l] %v");

    const auto logger = std::make_shared<spdlog::logger>("mantis_logger", console_sink);
    logger->set_level(spdlog::level::info);

    // Make `logger` the default logger
    spdlog::set_default_logger(logger);
}

mb::LogDatabase &mb::Logger::getLogsDb() {
    static LogDatabase logsDb;
    return logsDb;
}

void mb::Logger::initDb(const std::string &data_dir) {
    if (getLogsDb().init(data_dir))
        isDbInitialized = true;
}

void mb::Logger::logToDatabase(const std::string &level, const std::string &origin, const std::string &message, 
                               const std::string &details, const json &data) {
    if (isDbInitialized)
        getLogsDb().insertLog(level, origin, message, details, data);
}

void mb::Logger::trace(const std::string &origin, const std::string &message, const std::string &details, const json &data) {
    // Format message for console output
    std::string formatted_msg;
    if (details.empty()) {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {}", origin, message)
                          : fmt::format("[{}] {}\n\t— {}", origin, message, data.dump());
    } else {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {} - {}", origin, message, details)
                          : fmt::format("[{}] {} - {}\n\t— {}", origin, message, details, data.dump());
    }

    // Call private template method for spdlog console logging
    trace_spdlog("{}", formatted_msg);

    // Log to database with structured format
    logToDatabase("trace", origin, message, details, data);
}

void mb::Logger::info(const std::string &origin, const std::string &message, const std::string &details, const json &data) {
    // Format message for console output
    std::string formatted_msg;
    if (details.empty()) {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {}", origin, message)
                          : fmt::format("[{}] {}\n\t— {}", origin, message, data.dump());
    } else {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {} - {}", origin, message, details)
                          : fmt::format("[{}] {} - {}\n\t— {}", origin, message, details, data.dump());
    }

    // Call private template method for spdlog console logging
    info_spdlog("{}", formatted_msg);

    // Log to database with structured format
    logToDatabase("info", origin, message, details, data);
}

void mb::Logger::debug(const std::string &origin, const std::string &message, const std::string &details, const json &data) {
    // Format message for console output
    std::string formatted_msg;
    if (details.empty()) {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {}", origin, message)
                          : fmt::format("[{}] {}\n\t— {}", origin, message, data.dump());
    } else {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {} - {}", origin, message, details)
                          : fmt::format("[{}] {} - {}\n\t— {}", origin, message, details, data.dump());
    }

    // Call private template method for spdlog console logging
    debug_spdlog("{}", formatted_msg);

    // Log to database with structured format
    logToDatabase("debug", origin, message, details, data);
}

void mb::Logger::warn(const std::string &origin, const std::string &message, const std::string &details, const json &data) {
    // Format message for console output
    std::string formatted_msg;
    if (details.empty()) {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {}", origin, message)
                          : fmt::format("[{}] {}\n\t— {}", origin, message, data.dump());
    } else {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {} - {}", origin, message, details)
                          : fmt::format("[{}] {} - {}\n\t— {}", origin, message, details, data.dump());
    }

    // Call private template method for spdlog console logging
    warn_spdlog("{}", formatted_msg);

    // Log to database with structured format
    logToDatabase("warn", origin, message, details, data);
}

void mb::Logger::critical(const std::string &origin, const std::string &message, const std::string &details, const json &data) {
    // Format message for console output
    std::string formatted_msg;
    if (details.empty()) {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {}", origin, message)
                          : fmt::format("[{}] {}\n\t— {}", origin, message, data.dump());
    } else {
        formatted_msg = data.empty()
                          ? fmt::format("[{}] {} - {}", origin, message, details)
                          : fmt::format("[{}] {} - {}\n\t— {}", origin, message, details, data.dump());
    }

    // Call private template method for spdlog console logging
    critical_spdlog("{}", formatted_msg);

    // Log to database with structured format
    logToDatabase("critical", origin, message, details, data);
}

mb::FuncLogger::FuncLogger(std::string msg) : m_msg(std::move(msg)) {
    LogOrigin::trace("Function Entry", fmt::format("Enter: {}", m_msg));
}

mb::FuncLogger::~FuncLogger() {
    LogOrigin::trace("Function Exit", fmt::format("Exit:  {}", m_msg));
}
