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

void mb::Logger::initDb(const std::string& data_dir) {
    if (getLogsDb().init(data_dir))
        isDbInitialized = true;
}

void mb::Logger::logToDatabase(const std::string &level, const std::string &message, const json &data) {
    if (isDbInitialized)
        getLogsDb().insertLog(level, message, data);
}

void mb::Logger::trace(const std::string &msg, const json &data) {
    // Format message with JSON data if provided for console output
    const std::string formatted_msg = data.empty() ? msg : fmt::format("{}\n\t— {}", msg, data.dump());

    // Call private template method for spdlog console logging
    trace("{}", formatted_msg);

    // Log to database with message and JSON data
    logToDatabase("trace", msg, data);
}

void mb::Logger::info(const std::string &msg, const json &data) {
    // Format message with JSON data if provided for console output
    const std::string formatted_msg = data.empty() ? msg : fmt::format("{}\n\t— {}", msg, data.dump());

    // Call private template method for spdlog console logging
    info("{}", formatted_msg);

    // Log to database with message and JSON data
    logToDatabase("info", msg, data);
}

void mb::Logger::debug(const std::string &msg, const json &data) {
    // Format message with JSON data if provided for console output
    const std::string formatted_msg = data.empty() ? msg : fmt::format("{}\n\t— {}", msg, data.dump());

    // Call private template method for spdlog console logging
    debug("{}", formatted_msg);

    // Log to database with message and JSON data
    logToDatabase("debug", msg, data);
}

void mb::Logger::warn(const std::string &msg, const json &data) {
    // Format message with JSON data if provided for console output
    const std::string formatted_msg = data.empty() ? msg : fmt::format("{}\n\t— {}", msg, data.dump());

    // Call private template method for spdlog console logging
    warn("{}", formatted_msg);

    // Log to database with message and JSON data
    logToDatabase("warn", msg, data);
}

void mb::Logger::critical(const std::string &msg, const json &data) {
    // Format message with JSON data if provided for console output
    const std::string formatted_msg = data.empty() ? msg : fmt::format("{}\n\t— {}", msg, data.dump());

    // Call private template method for spdlog console logging
    critical("{}", formatted_msg);

    // Log to database with message and JSON data
    logToDatabase("critical", msg, data);
}

mb::FuncLogger::FuncLogger(std::string msg) : m_msg(std::move(msg)) {
    logger::trace(fmt::format("Enter: {}", m_msg));
}

mb::FuncLogger::~FuncLogger() {
    logger::trace(fmt::format("Exit:  {}", m_msg));
}
