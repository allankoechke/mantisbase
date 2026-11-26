#include "../../include/mantisbase/utils/utils.h"
#include <private/soci-mktime.h>

#include <ctime>

#include "../../include/mantisbase/mantisbase.h"

namespace mb
{
    std::tm toUtcTime(const std::time_t t)
    {
        // gmtime() writes a process-wide buffer. The server thread and a
        // request thread both format timestamps, and on Windows that race
        // faults. The reentrant call fills a private tm.
        std::tm out{};
#if defined(_WIN32)
        if (gmtime_s(&out, &t) != 0)
            return {};
#else
        if (gmtime_r(&t, &out) == nullptr)
            return {};
#endif
        return out;
    }

    std::tm toLocalTime(const std::time_t t)
    {
        std::tm out{};
#if defined(_WIN32)
        if (localtime_s(&out, &t) != 0)
            return {};
#else
        if (localtime_r(&t, &out) == nullptr)
            return {};
#endif
        return out;
    }

    std::string tmToStr(const std::tm& t)
    {
        char buffer[80];
        const int length = soci::details::format_std_tm(t, buffer, sizeof(buffer));
        std::string iso_string(buffer, length);
        return iso_string;
    }

    std::tm strToTM(const std::string& value)
    {
        auto dt = value;
        std::tm t;

        auto pos = dt.find('T');
        if (pos != std::string::npos) dt[pos] = ' ';
        soci::details::parse_std_tm(dt.c_str(), t);

        return t;
    }

    std::string dbDateToString(const std::string& db_type, const soci::row& row, const int index)
    {
        if (db_type == "sqlite3")
        {
            // Treat date as a string by default
            return row.get<std::string>(index);
        }

        // if (row.get_properties(index).get_db_type() == soci::db_date)
        const auto t = row.get<std::tm>(index);
        return tmToStr(t);
    }
}
