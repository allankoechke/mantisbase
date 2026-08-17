/**
 * @file int_precision.h
 * @brief Signed/unsigned integer width tokens for entity int fields (i8..u64).
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

    [[nodiscard]] IntPrecision defaultIntPrecision();

    [[nodiscard]] bool isUnsignedIntPrecision(IntPrecision precision);

    [[nodiscard]] std::string intPrecisionToString(IntPrecision precision);

    [[nodiscard]] IntPrecision intPrecisionFromString(const std::string &token);

    /**
     * Parse precision from a field JSON object.
     * Accepts string tokens (i8, u32, ...) or legacy numeric bit widths (8, 16, 32, 64) as signed.
     */
    [[nodiscard]] IntPrecision intPrecisionFromField(const nlohmann::json &field);

    [[nodiscard]] soci::db_type intPrecisionToSociType(IntPrecision precision);

    [[nodiscard]] std::pair<int64_t, int64_t> intPrecisionSignedRange(IntPrecision precision);

    [[nodiscard]] std::pair<uint64_t, uint64_t> intPrecisionUnsignedRange(IntPrecision precision);

    [[nodiscard]] std::optional<std::string> validateIntPrecisionToken(const std::string &token);

    [[nodiscard]] std::optional<std::string> validateIntFieldValue(const nlohmann::json &value,
                                                                   IntPrecision precision);

    [[nodiscard]] std::optional<std::string> validateIntConstraintBounds(const nlohmann::json &field);

    void bindIntFieldValue(soci::values &vals, const std::string &field_name, const nlohmann::json &value,
                           IntPrecision precision);

    [[nodiscard]] nlohmann::json readIntFieldValue(const soci::row &row, size_t index, IntPrecision precision);
} // namespace mb

#endif // MANTISBASE_INT_PRECISION_H
