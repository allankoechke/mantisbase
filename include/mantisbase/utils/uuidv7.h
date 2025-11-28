//
// Created by codeart on 09/11/2025.
//

#ifndef MANTISAPP_UUIDV7_H
#define MANTISAPP_UUIDV7_H

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <cstdint>
#include <mutex>

namespace mantis {
    inline uint64_t now_unix_ms() {
        using namespace std::chrono;
        return static_cast<uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
    }

    inline std::string to_hex_lower(const std::array<uint8_t,16>& b) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < 16; ++i) {
            ss << std::setw(2) << static_cast<int>(b[i]);
            if (i == 3 || i == 5 || i == 7 || i == 9) ss << '-';
        }
        return ss.str();
    }

    inline std::string generate_uuidv7() {
        thread_local std::mt19937_64 rng((std::random_device{})());
        thread_local uint64_t last_ts = 0;
        thread_local uint16_t counter = 0;

        const uint64_t ts_ms = now_unix_ms() & 0x0000FFFFFFFFFFFFULL; // 48 bits

        if (ts_ms == last_ts) {
            ++counter;
        } else {
            last_ts = ts_ms;
            counter = 0;
        }

        // 12-bit random or counter value (rand_a)
        const uint16_t rand12 = counter & 0x0FFF; // 12 bits, wraps automatically

        // 62-bit random (rand_b)
        std::uniform_int_distribution<uint64_t> dist62(0, ( (1ULL<<62) - 1 ));
        const uint64_t rand62 = dist62(rng);

        std::array<uint8_t,16> bytes{};
        for (int i = 0; i < 6; ++i)
            bytes[i] = static_cast<uint8_t>((ts_ms >> (8*(5-i))) & 0xFF);

        bytes[6] = static_cast<uint8_t>((7u << 4) | ((rand12 >> 8) & 0x0F)); // version 7
        bytes[7] = static_cast<uint8_t>(rand12 & 0xFF);

        bytes[8] = static_cast<uint8_t>(0x80 | ((rand62 >> 56) & 0x3F)); // variant
        for (int i = 0; i < 7; ++i)
            bytes[9 + i] = static_cast<uint8_t>((rand62 >> (48 - 8*i)) & 0xFF);

        return to_hex_lower(bytes);
    }
}

#endif //MANTISAPP_UUIDV7_H