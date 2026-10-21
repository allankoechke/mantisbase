#include "../../../include/mantisbase/core/models/int_precision.h"

#include "../../../include/mantisbase/core/exceptions.h"
#include "../../../include/mantisbase/utils/utils.h"

#include <format>
#include <limits>

namespace mb {
    MbIntPrecision defaultIntPrecision() {
        return MbIntPrecision::I32;
    }

    bool isUnsignedIntPrecision(const MbIntPrecision precision) {
        switch (precision) {
            case MbIntPrecision::U8:
            case MbIntPrecision::U16:
            case MbIntPrecision::U32:
            case MbIntPrecision::U64:
                return true;
            default:
                return false;
        }
    }

    std::string intPrecisionToString(const MbIntPrecision precision) {
        switch (precision) {
            case MbIntPrecision::I8: return "i8";
            case MbIntPrecision::I16: return "i16";
            case MbIntPrecision::I32: return "i32";
            case MbIntPrecision::I64: return "i64";
            case MbIntPrecision::U8: return "u8";
            case MbIntPrecision::U16: return "u16";
            case MbIntPrecision::U32: return "u32";
            case MbIntPrecision::U64: return "u64";
        }
        return "i32";
    }

    MbIntPrecision intPrecisionFromString(const std::string &token) {
        auto normalized = trim(token);
        toLowerCase(normalized);
        if (normalized == "i8") return MbIntPrecision::I8;
        if (normalized == "i16") return MbIntPrecision::I16;
        if (normalized == "i32") return MbIntPrecision::I32;
        if (normalized == "i64") return MbIntPrecision::I64;
        if (normalized == "u8") return MbIntPrecision::U8;
        if (normalized == "u16") return MbIntPrecision::U16;
        if (normalized == "u32") return MbIntPrecision::U32;
        if (normalized == "u64") return MbIntPrecision::U64;
        throw MbException(400,
                              "Invalid precision `" + token + "`. Must be one of: "
                              "i8, i16, i32, i64, u8, u16, u32, u64.");
    }

    namespace {
        MbIntPrecision legacyBitWidthToPrecision(const int bits) {
            switch (bits) {
                case 8: return MbIntPrecision::I8;
                case 16: return MbIntPrecision::I16;
                case 64: return MbIntPrecision::I64;
                case 32: return MbIntPrecision::I32;
                default:
                    throw MbException(400,
                                          "Invalid legacy precision `" + std::to_string(bits)
                                          + "`. Must be 8, 16, 32, or 64, or use i8/u8 tokens.");
            }
        }
    }

    MbIntPrecision intPrecisionFromField(const nlohmann::json &field) {
        if (!field.contains("precision")) {
            return defaultIntPrecision();
        }

        const auto &precision = field.at("precision");
        if (precision.is_string()) {
            return intPrecisionFromString(precision.get<std::string>());
        }
        if (precision.is_number_integer()) {
            return legacyBitWidthToPrecision(precision.get<int>());
        }

        throw MbException(400,
                              "Expected a string precision token (i8, u32, ...) or legacy integer bit width.");
    }

    soci::db_type intPrecisionToSociType(const MbIntPrecision precision) {
        switch (precision) {
            case MbIntPrecision::I8: return soci::db_int8;
            case MbIntPrecision::I16: return soci::db_int16;
            case MbIntPrecision::I32: return soci::db_int32;
            case MbIntPrecision::I64: return soci::db_int64;
            case MbIntPrecision::U8: return soci::db_uint8;
            case MbIntPrecision::U16: return soci::db_uint16;
            case MbIntPrecision::U32: return soci::db_uint32;
            case MbIntPrecision::U64: return soci::db_uint64;
        }
        return soci::db_int32;
    }

    std::pair<int64_t, int64_t> intPrecisionSignedRange(const MbIntPrecision precision) {
        switch (precision) {
            case MbIntPrecision::I8: return {std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max()};
            case MbIntPrecision::I16: return {std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()};
            case MbIntPrecision::I32: return {std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()};
            case MbIntPrecision::I64: return {std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max()};
            default:
                throw MbException(500, "Signed range requested for unsigned precision.");
        }
    }

    std::pair<uint64_t, uint64_t> intPrecisionUnsignedRange(const MbIntPrecision precision) {
        switch (precision) {
            case MbIntPrecision::U8: return {0, std::numeric_limits<uint8_t>::max()};
            case MbIntPrecision::U16: return {0, std::numeric_limits<uint16_t>::max()};
            case MbIntPrecision::U32: return {0, std::numeric_limits<uint32_t>::max()};
            case MbIntPrecision::U64: return {0, std::numeric_limits<uint64_t>::max()};
            default:
                throw MbException(500, "Unsigned range requested for signed precision.");
        }
    }

    std::optional<std::string> validateIntPrecisionToken(const std::string &token) {
        try {
            (void)intPrecisionFromString(token);
            return std::nullopt;
        } catch (const MbException &e) {
            return e.what();
        }
    }

    namespace {
        std::optional<uint64_t> jsonToUnsignedInt(const nlohmann::json &value) {
            if (value.is_number_unsigned()) {
                return value.get<uint64_t>();
            }
            if (value.is_number_integer()) {
                const auto v = value.get<int64_t>();
                if (v < 0) {
                    return std::nullopt;
                }
                return static_cast<uint64_t>(v);
            }
            return std::nullopt;
        }

        std::optional<int64_t> jsonToSignedInt(const nlohmann::json &value) {
            if (value.is_number_integer()) {
                return value.get<int64_t>();
            }
            return std::nullopt;
        }
    }

    std::optional<std::string> validateIntFieldValue(const nlohmann::json &value, const MbIntPrecision precision) {
        if (value.is_null()) {
            return std::nullopt;
        }

        if (isUnsignedIntPrecision(precision)) {
            const auto parsed = jsonToUnsignedInt(value);
            if (!parsed.has_value()) {
                return "Value must be a non-negative integer.";
            }

            const auto [min_v, max_v] = intPrecisionUnsignedRange(precision);
            if (*parsed < min_v || *parsed > max_v) {
                return std::format("Value must be between {} and {} for precision `{}`.",
                                   min_v, max_v, intPrecisionToString(precision));
            }
            return std::nullopt;
        }

        const auto parsed = jsonToSignedInt(value);
        if (!parsed.has_value()) {
            return "Value must be an integer.";
        }

        const auto [min_v, max_v] = intPrecisionSignedRange(precision);
        if (*parsed < min_v || *parsed > max_v) {
            return std::format("Value must be between {} and {} for precision `{}`.",
                               min_v, max_v, intPrecisionToString(precision));
        }

        return std::nullopt;
    }

    std::optional<std::string> validateIntConstraintBounds(const nlohmann::json &field) {
        if (!field.contains("type") || field.at("type").get<std::string>() != "int") {
            return std::nullopt;
        }

        const auto precision = intPrecisionFromField(field);
        const auto &constraints = field.value("constraints", nlohmann::json::object());
        const auto &field_name = field.at("name").get<std::string>();

        if (!constraints.is_object()) {
            return std::nullopt;
        }

        const auto validate_bound = [&](const char *key) -> std::optional<std::string> {
            if (!constraints.contains(key) || constraints.at(key).is_null()) {
                return std::nullopt;
            }

            const auto &bound = constraints.at(key);
            if (isUnsignedIntPrecision(precision)) {
                const auto parsed = jsonToUnsignedInt(bound);
                if (!parsed.has_value()) {
                    return std::format("Constraint `{}` for field `{}` must be a non-negative integer.", key,
                                       field_name);
                }
                const auto [min_v, max_v] = intPrecisionUnsignedRange(precision);
                if (*parsed < min_v || *parsed > max_v) {
                    return std::format("Constraint `{}` for field `{}` must be between {} and {} for precision `{}`.",
                                       key, field_name, min_v, max_v, intPrecisionToString(precision));
                }
                return std::nullopt;
            }

            const auto parsed = jsonToSignedInt(bound);
            if (!parsed.has_value()) {
                return std::format("Constraint `{}` for field `{}` must be an integer.", key, field_name);
            }

            const auto [min_v, max_v] = intPrecisionSignedRange(precision);
            if (*parsed < min_v || *parsed > max_v) {
                return std::format("Constraint `{}` for field `{}` must be between {} and {} for precision `{}`.",
                                   key, field_name, min_v, max_v, intPrecisionToString(precision));
            }
            return std::nullopt;
        };

        if (const auto err = validate_bound("min_value"); err.has_value()) {
            return err;
        }
        if (const auto err = validate_bound("max_value"); err.has_value()) {
            return err;
        }

        if (constraints.contains("min_value") && constraints.contains("max_value")
            && !constraints.at("min_value").is_null() && !constraints.at("max_value").is_null()) {
            if (isUnsignedIntPrecision(precision)) {
                const auto min_v = jsonToUnsignedInt(constraints.at("min_value")).value_or(0);
                const auto max_v = jsonToUnsignedInt(constraints.at("max_value")).value_or(0);
                if (min_v > max_v) {
                    return std::format("Constraint `min_value` must be <= `max_value` for field `{}`.", field_name);
                }
            } else {
                const auto min_v = jsonToSignedInt(constraints.at("min_value")).value_or(0);
                const auto max_v = jsonToSignedInt(constraints.at("max_value")).value_or(0);
                if (min_v > max_v) {
                    return std::format("Constraint `min_value` must be <= `max_value` for field `{}`.", field_name);
                }
            }
        }

        return std::nullopt;
    }

    void bindIntFieldValue(soci::values &vals, const std::string &field_name, const nlohmann::json &value,
                           const MbIntPrecision precision) {
        if (isUnsignedIntPrecision(precision)) {
            const auto parsed = jsonToUnsignedInt(value).value_or(0);
            switch (precision) {
                case MbIntPrecision::U8:
                    vals.set(field_name, static_cast<uint8_t>(parsed));
                    break;
                case MbIntPrecision::U16:
                    vals.set(field_name, static_cast<uint16_t>(parsed));
                    break;
                case MbIntPrecision::U32:
                    vals.set(field_name, static_cast<uint32_t>(parsed));
                    break;
                case MbIntPrecision::U64:
                    vals.set(field_name, parsed);
                    break;
                default:
                    break;
            }
            return;
        }

        const auto parsed = jsonToSignedInt(value).value_or(0);
        switch (precision) {
            case MbIntPrecision::I8:
                vals.set(field_name, static_cast<int8_t>(parsed));
                break;
            case MbIntPrecision::I16:
                vals.set(field_name, static_cast<int16_t>(parsed));
                break;
            case MbIntPrecision::I32:
                vals.set(field_name, static_cast<int32_t>(parsed));
                break;
            case MbIntPrecision::I64:
                vals.set(field_name, parsed);
                break;
            default:
                break;
        }
    }

    nlohmann::json readIntFieldValue(const soci::row &row, const size_t index, const MbIntPrecision precision) {
        switch (precision) {
            case MbIntPrecision::I8: return row.get<int8_t>(index);
            case MbIntPrecision::I16: return row.get<int16_t>(index);
            case MbIntPrecision::I32: return row.get<int32_t>(index);
            case MbIntPrecision::I64: return row.get<int64_t>(index);
            case MbIntPrecision::U8: return row.get<uint8_t>(index);
            case MbIntPrecision::U16: return row.get<uint16_t>(index);
            case MbIntPrecision::U32: return row.get<uint32_t>(index);
            case MbIntPrecision::U64: return row.get<uint64_t>(index);
        }
        return row.get<int32_t>(index);
    }
} // namespace mb
