/**
 * @file soci_custom_types.hpp
 * @brief Extend base soci types
 *
 * This file adds `boolean` and `json` types to soci.
 */

#ifndef SOCI_CUSTOM_TYPES_H
#define SOCI_CUSTOM_TYPES_H

#include <soci/soci.h>
#include <nlohmann/json.hpp>
#include "../../utils/utils.h"

/**
 * @brief Add SOCI support for `booleans` and `json` types
 *
 * SOCI docs say that for each data type we need to support, we have to implement a `struct` having two methods:
 * - `from_base(...)` to convert from base type to intended type
 * - `to_base(...)` to convert from our types to base types.
 *
 * Checkout soci docs here: @see https://soci.sourceforge.net/doc/master/types/#user-defined-c-types
 */
namespace soci {
    using json = nlohmann::json;

    /**
     * @brief Boolean type conversion to base soci::type
     * This will allow us to cast to/from boolean types when dealing with soci.
     */
    template <>
    struct type_conversion<bool> {
        typedef uint16_t base_type;

        static void from_base(uint16_t i, indicator ind, bool & b) {
            if (ind == i_null) {
                b = false;
                return;
            }
            b = (i != 0);
        }

        static void to_base(const bool & b, uint16_t & i, indicator & ind) {
            i = b ? 1 : 0;
            ind = i_ok;
        }
    };

    /**
     * @brief JSON type conversion to base soci::type
     * This will allow us to cast to/from json types when dealing with soci.
     */
    template <>
    struct type_conversion<json> {
        typedef std::string base_type;

        static void from_base(const std::string & s, indicator ind, json & jb) {
            if (ind == i_null) {
                jb = json{}; // or handle null as appropriate
                return;
            }
            jb = json::parse(s); // or however you parse JSON
        }

        static void to_base(const json & json, std::string & s, indicator & ind) {
            s = json.dump();
            ind = i_ok;
        }
    };
}

#endif //SOCI_CUSTOM_TYPES_H
