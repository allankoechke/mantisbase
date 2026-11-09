#ifndef MANTISBASE_EMAIL_H
#define MANTISBASE_EMAIL_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>

#include "types.h"

namespace mb {
    /**
     * Outbound SMTP settings. `use_tls` on port 465 opens an implicit TLS
     * connection. Any other port with `use_tls` negotiates STARTTLS. With
     * `use_tls` false the session stays in cleartext.
     */
    struct MbSmtpConfig {
        std::string host;
        int port = 587;
        std::string user;
        std::string password;
        std::string from_address;
        bool use_tls = true;

        [[nodiscard]] bool isValid() const;
        [[nodiscard]] bool isMock() const;

        /** Read the settings `smtp` object (`host`, `port`, `user`, `password`, `from`, `tls`). */
        [[nodiscard]] static MbSmtpConfig fromJson(const json &smtp);
    };

    struct MbEmailMsg {
        std::string to;
        std::string subject;
        std::string body_html;
        std::string body_text;
    };

    struct MbRateLimitEntry {
        std::string email;
        std::vector<std::chrono::steady_clock::time_point> timestamps;
    };

    class MbMailer : public MbAppMixin {
    public:
        explicit MbMailer(const MantisBase &app);

        void configure(const MbSmtpConfig &config);

        /**
         * Load the `smtp` object from application settings.
         * An empty or incomplete host leaves the mailer unconfigured.
         */
        void applySettings(const json &configs);

        const MbSmtpConfig &config() const;

        bool isConfigured() const;

        /**
         * Deliver a message. Mock hosts (`mock`, `localhost:0`) capture it in
         * sentMessages() and do not connect. Otherwise the message is sent
         * over SMTP on a trantor TCP connection.
         * Throws MbException on validation errors, rate limits, or delivery failure.
         */
        bool send(const MbEmailMsg &message);

        /**
         * Send a fixed "Hey, it works!" message to `to`.
         * Throws MbException when SMTP is not configured, the address is invalid,
         * the rate limit is exceeded, or delivery fails.
         */
        bool sendTest(const std::string &to);

        /** Subject and body used by sendTest(). */
        static constexpr std::string_view kTestMessage{"MantisBase: Hey, it works!"};

        std::string loadTemplate(const std::string &template_name,
                                  const std::unordered_map<std::string, std::string> &variables) const;

        bool checkRateLimit(const std::string &email, int max_per_hour = 3);

        /** Messages captured by the mock transport. */
        const std::vector<MbEmailMsg> &sentMessages() const;

        void clearSentMessages();

        void clearRateLimits();

        static std::string verificationEmailHtml(const std::string &app_name,
                                                   const std::string &verify_url);

        static std::string verificationEmailText(const std::string &app_name,
                                                   const std::string &verify_url);

        static std::string passwordResetEmailHtml(const std::string &app_name,
                                                     const std::string &reset_url);

        static std::string passwordResetEmailText(const std::string &app_name,
                                                     const std::string &reset_url);

    private:
        /**
         * Take user email and hide some chars for logging
         *
         * @param email User email to hide
         * @return Partial email for log publishing
         */
        static std::string hideEmail(const std::string &email);

        MbSmtpConfig m_config;
        bool m_configured = false;

        mutable std::mutex m_rate_limit_mutex;
        std::unordered_map<std::string, MbRateLimitEntry> m_rate_limits;

        std::vector<MbEmailMsg> m_sent_messages;
    };
} // namespace mb

#endif //MANTISBASE_EMAIL_H
