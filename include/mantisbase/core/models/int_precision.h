/**
 * @file int_precision.h
 * @brief Signed/unsigned integer width tokens for entity int fields (i8..u64).
 *
 * Entity `int` fields store a precision token (`i32`, `u64`, …) that controls
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

namespace mb {
    /** Storage width and signedness for entity `int` fields. */
    enum class IntPrecision {
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
    [[nodiscard]] IntPrecision defaultIntPrecision();

    /** @return `true` for `U8`..`U64` precisions. */
    [[nodiscard]] bool isUnsignedIntPrecision(IntPrecision precision);

    /** @return String token such as `"i32"` or `"u64"`. */
    [[nodiscard]] std::string intPrecisionToString(IntPrecision precision);

    /** Parse a precision token; throws on unknown input. */
    [[nodiscard]] IntPrecision intPrecisionFromString(const std::string &token);

    /**
     * Parse precision from a field JSON object.
     * Accepts string tokens (i8, u32, …) or legacy numeric bit widths (8, 16, 32, 64) as signed.
     */
    [[nodiscard]] IntPrecision intPrecisionFromField(const nlohmann::json &field);

    /** Map precision to the SOCI indicator type used for bind/read. */
    [[nodiscard]] soci::db_type intPrecisionToSociType(IntPrecision precision);

    /** @return `{min, max}` inclusive range for signed precisions. */
    [[nodiscard]] std::pair<int64_t, int64_t> intPrecisionSignedRange(IntPrecision precision);

    /** @return `{min, max}` inclusive range for unsigned precisions. */
    [[nodiscard]] std::pair<uint64_t, uint64_t> intPrecisionUnsignedRange(IntPrecision precision);

    /** @return Error message if `token` is not a valid precision string, else `nullopt`. */
    [[nodiscard]] std::optional<std::string> validateIntPrecisionToken(const std::string &token);

    /** @return Error message if `value` is out of range for `precision`, else `nullopt`. */
    [[nodiscard]] std::optional<std::string> validateIntFieldValue(const nlohmann::json &value,
                                                                   IntPrecision precision);

    /** Validate `min`/`max` constraint keys on an int field definition. */
    [[nodiscard]] std::optional<std::string> validateIntConstraintBounds(const nlohmann::json &field);

    /** Bind an int JSON value into SOCI `vals` using the correct width/type. */
    void bindIntFieldValue(soci::values &vals, const std::string &field_name, const nlohmann::json &value,
                           IntPrecision precision);

    /** Read an int column from a SOCI row into JSON using the field precision. */
    [[nodiscard]] nlohmann::json readIntFieldValue(const soci::row &row, size_t index, IntPrecision precision);
} // namespace mb

#endif // MANTISBASE_INT_PRECISION_H
