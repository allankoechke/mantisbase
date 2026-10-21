/**
 * @file int_precision.h
 * @brief Signed/unsigned integer width tokens for entity int fields (i8..u64).
 *
 * MbEntity `int` fields store a precision token (`i32`, `u64`, …) that controls
 * SOCI binding, SQL column type, and validation range checks.
 */

#ifndef MANTISBASE_INT_PRECISION_H
#define MANTISBASE_INT_PRECISION_H

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "nlohmann/json.hpp"
#include "soci/soci-backend.h"
#include "soci/values.h"
#include "../types.h"

namespace mb {
    /** Storage width and signedness for entity `int` fields. */
    enum class MbIntPrecision {
        I8,
        I16,
        I32,
        I64,
        U8,
        U16,
        U32,
        U64,
    };

    /** @return Default precision (`I32`) for int fields without an explicit token. */
    [[nodiscard]] MANTISBASE_API MbIntPrecision defaultIntPrecision();

    /** @return `true` for `U8`..`U64` precisions. */
    [[nodiscard]] MANTISBASE_API bool isUnsignedIntPrecision(MbIntPrecision precision);

    /** @return String token such as `"i32"` or `"u64"`. */
    [[nodiscard]] MANTISBASE_API std::string intPrecisionToString(MbIntPrecision precision);

    /** Parse a precision token; throws on unknown input. */
    [[nodiscard]] MANTISBASE_API MbIntPrecision intPrecisionFromString(const std::string &token);

    /**
     * Parse precision from a field JSON object.
     * Accepts string tokens (i8, u32, …) or legacy numeric bit widths (8, 16, 32, 64) as signed.
     */
    [[nodiscard]] MANTISBASE_API MbIntPrecision intPrecisionFromField(const nlohmann::json &field);

    /** Map precision to the SOCI indicator type used for bind/read. */
    [[nodiscard]] MANTISBASE_API soci::db_type intPrecisionToSociType(MbIntPrecision precision);

    /** @return `{min, max}` inclusive range for signed precisions. */
    [[nodiscard]] MANTISBASE_API std::pair<int64_t, int64_t> intPrecisionSignedRange(MbIntPrecision precision);

    /** @return `{min, max}` inclusive range for unsigned precisions. */
    [[nodiscard]] MANTISBASE_API std::pair<uint64_t, uint64_t> intPrecisionUnsignedRange(MbIntPrecision precision);

    /** @return Error message if `token` is not a valid precision string, else `nullopt`. */
    [[nodiscard]] MANTISBASE_API std::optional<std::string> validateIntPrecisionToken(const std::string &token);

    /** @return Error message if `value` is out of range for `precision`, else `nullopt`. */
    [[nodiscard]] MANTISBASE_API std::optional<std::string> validateIntFieldValue(const nlohmann::json &value,
                                                                   MbIntPrecision precision);

    /** Validate `min`/`max` constraint keys on an int field definition. */
    [[nodiscard]] MANTISBASE_API std::optional<std::string> validateIntConstraintBounds(const nlohmann::json &field);

    /** Bind an int JSON value into SOCI `vals` using the correct width/type. */
    MANTISBASE_API void bindIntFieldValue(soci::values &vals, const std::string &field_name, const nlohmann::json &value,
                           MbIntPrecision precision);

    /** Read an int column from a SOCI row into JSON using the field precision. */
    [[nodiscard]] MANTISBASE_API nlohmann::json readIntFieldValue(const soci::row &row, size_t index, MbIntPrecision precision);
} // namespace mb

#endif // MANTISBASE_INT_PRECISION_H
