#include "../../include/mantisbase/utils/utils.h"
#include <private/soci-mktime.h>

#include <ctime>
#include <mutex>

#include "../../include/mantisbase/mantisbase.h"

namespace mb
{
    std::tm toUtcTime(const std::time_t t)
    {
        // std::gmtime writes into a shared static buffer, so concurrent calls
        // from request threads race on it. Serialize the call and copy the
        // result out while holding the lock.
        static std::mutex mtx;
        std::lock_guard lock(mtx);
        return *std::gmtime(&t);
    }

    std::tm toLocalTime(const std::time_t t)
    {
        // std::localtime writes into a shared static buffer, so concurrent
        // calls from request threads race on it. Serialize the call and copy
        // the result out while holding the lock.
        static std::mutex mtx;
        std::lock_guard lock(mtx);
        return *std::localtime(&t);
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

    std::string dbDateToString(const MantisBase& app, const soci::row& row, const int index)
    {
        if (const std::string& db_type = app.dbType(); db_type == "sqlite3")
        {
            // Treat date as a string by default
            return row.get<std::string>(index);
        }

        // if (row.get_properties(index).get_db_type() == soci::db_date)
        const auto t = row.get<std::tm>(index);
        return tmToStr(t);
    }
}
