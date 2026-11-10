/**
 * @file auth_methods.h
 * @brief Per-entity login method flags (password, OTP, OAuth).
 */

#ifndef MANTISBASE_AUTH_METHODS_H
#define MANTISBASE_AUTH_METHODS_H

#include <nlohmann/json.hpp>
#include <mantisbase/core/exceptions.h>
#include <mantisbase/export.h>

namespace mb {
    /**
     * @brief Which login methods an auth entity accepts.
     *
 * Password login is enabled by default. Email OTP and OAuth are disabled
 * until an admin turns them on through the entity schema.
 * `requireEmailVerification` defaults off so existing accounts can still sign in.
 */
struct MANTISBASE_API MbAuthMethods {
    bool password = true;
    bool otp = false;
    bool oauth = false;
    bool requireEmailVerification = false;

    [[nodiscard]] nlohmann::json toJSON() const {
        return {
            {"password", password},
            {"otp", otp},
            {"oauth", oauth},
            {"requireEmailVerification", requireEmailVerification}
        };
    }

        /** Overlay booleans from an `auth` object. Invalid values are ignored. */
        [[nodiscard]] static MbAuthMethods fromJSON(const nlohmann::json &auth) {
            MbAuthMethods methods;
            if (!auth.is_object())
                return methods;
            auto take = [&](const char *key, bool &dest) {
                if (auth.contains(key) && auth[key].is_boolean())
                    dest = auth[key].get<bool>();
            };
            take("password", methods.password);
            take("otp", methods.otp);
            take("oauth", methods.oauth);
            take("requireEmailVerification", methods.requireEmailVerification);
            return methods;
        }

        /**
         * Merge a strict `auth` object into this value.
         * Unknown keys and non-booleans throw @ref MbException 400.
         * Omitted keys keep their current value.
         */
        void mergeStrict(const nlohmann::json &auth) {
            if (!auth.is_object())
                throw MbException(400, "Expected `auth` to be an object.");
            for (const auto &[key, value]: auth.items()) {
                if (key != "password" && key != "otp" && key != "oauth" && key != "requireEmailVerification")
                    throw MbException(400, "Unknown auth setting `" + key + "`.");
                if (!value.is_boolean())
                    throw MbException(400, "Expected `auth." + key + "` to be a boolean.");
                if (key == "password")
                    password = value.get<bool>();
                else if (key == "otp")
                    otp = value.get<bool>();
                else if (key == "oauth")
                    oauth = value.get<bool>();
                else
                    requireEmailVerification = value.get<bool>();
            }
        }
    };
} // mb

#endif // MANTISBASE_AUTH_METHODS_H
