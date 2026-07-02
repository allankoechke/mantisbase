//
// https://github.com/sniper00/snowflake-cpp.git
// using snowflake_t = Snowflake<1534832906275L>; //
// using snowflake_t = Snowflake<1577836800000L>; // Jan 1st 2020
// snowflake_t uuid;
// uuid.init(1, 1);
//
// for (int64_t i = 0; i < 10000; ++i)
// {
//     auto id = uuid.nextID();
//     std::cout << id << "\n";
// }
//

#ifndef MANTISBASE_SNOWFLAKE_HPP
#define MANTISBASE_SNOWFLAKE_HPP

#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <mutex>

class SnowflakeNonLock
{
public:
    void lock() {}
    void unlock() {}
};

template<int64_t Twepoch, typename Lock = SnowflakeNonLock>
class Snowflake
{
    using lock_type = Lock;
    static constexpr int64_t TWEPOCH = Twepoch;
    static constexpr int64_t WORKER_ID_BITS = 5L;
    static constexpr int64_t DATACENTER_ID_BITS = 5L;
    static constexpr int64_t MAX_WORKER_ID = (1 << WORKER_ID_BITS) - 1;
    static constexpr int64_t MAX_DATACENTER_ID = (1 << DATACENTER_ID_BITS) - 1;
    static constexpr int64_t SEQUENCE_BITS = 12L;
    static constexpr int64_t WORKER_ID_SHIFT = SEQUENCE_BITS;
    static constexpr int64_t DATACENTER_ID_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS;
    static constexpr int64_t TIMESTAMP_LEFT_SHIFT = SEQUENCE_BITS + WORKER_ID_BITS + DATACENTER_ID_BITS;
    static constexpr int64_t SEQUENCE_MASK = (1 << SEQUENCE_BITS) - 1;

    using time_point = std::chrono::time_point<std::chrono::steady_clock>;

    time_point m_start_time_point = std::chrono::steady_clock::now();
    int64_t m_start_millisecond = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    int64_t m_last_timestamp = -1;
    int64_t m_worker_id = 0;
    int64_t m_datacenter_id = 0;
    int64_t m_sequence = 0;
    lock_type m_lock;

public:
    Snowflake() = default;

    Snowflake(const Snowflake&) = delete;

    Snowflake& operator=(const Snowflake&) = delete;

    void init(const int64_t worker_id, const int64_t datacenter_id)
    {
        if (worker_id > MAX_WORKER_ID || worker_id < 0) {
            throw std::runtime_error("worker Id can't be greater than 31 or less than 0");
        }

        if (datacenter_id > MAX_DATACENTER_ID || datacenter_id < 0) {
            throw std::runtime_error("datacenter Id can't be greater than 31 or less than 0");
        }

        m_worker_id = worker_id;
        m_datacenter_id = datacenter_id;
    }

    int64_t nextID()
    {
        std::lock_guard<lock_type> lock(m_lock);
        // std::chrono::steady_clock  cannot decrease as physical time moves forward
        auto timestamp = millisecond();
        if (m_last_timestamp == timestamp)
        {
            m_sequence = (m_sequence + 1)&SEQUENCE_MASK;
            if (m_sequence == 0)
            {
                timestamp = waitNextMillis(m_last_timestamp);
            }
        }
        else
        {
            m_sequence = 0;
        }

        m_last_timestamp = timestamp;

        return ((timestamp - TWEPOCH) << TIMESTAMP_LEFT_SHIFT)
            | (m_datacenter_id << DATACENTER_ID_SHIFT)
            | (m_worker_id << WORKER_ID_SHIFT)
            | m_sequence;
    }

private:
    [[nodiscard]] int64_t millisecond() const noexcept
    {
        const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_start_time_point);
        return m_start_millisecond + diff.count();
    }

    [[nodiscard]] int64_t waitNextMillis(const int64_t last) const noexcept
    {
        auto timestamp = millisecond();
        while (timestamp <= last)
        {
            timestamp = millisecond();
        }
        return timestamp;
    }
};

#endif //MANTISBASE_SNOWFLAKE_HPP