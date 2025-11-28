#include "../../include/mantisbase/core/logger.h"
#include <spdlog/sinks/stdout_color_sinks-inl.h>
#include "spdlog/sinks/ansicolor_sink.h"

void mantis::LogsMgr::setLogLevel(const LogLevel& level)
{
    const auto set_spdlog_level = [&](const spdlog::level::level_enum lvl)
    {
        const auto logger = spdlog::get("mantis_logger");
        logger->set_level(lvl);

        // Also set sink levels
        for (const auto& sink : logger->sinks()) {
            sink->set_level(lvl);
        }
    };

    switch (level)
    {
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

void mantis::LogsMgr::init()
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%-8l] %v");

    const auto logger = std::make_shared<spdlog::logger>("mantis_logger", console_sink);
    logger->set_level(spdlog::level::info);

    // Make `logger` the default logger
    spdlog::set_default_logger(logger);
}

mantis::FuncLogger::FuncLogger(const std::string& msg): m_msg(msg)
{
    logger::trace("Enter: {}", m_msg);
}

mantis::FuncLogger::~FuncLogger()
{
    logger::trace("Exit:  {}", m_msg);
}
