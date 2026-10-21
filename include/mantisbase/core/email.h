#ifndef MANTISBASE_EMAIL_H
#define MANTISBASE_EMAIL_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>

#include "types.h"

namespace mb {
    struct MbSmtpConfig {
        std::string host;
        int port = 587;
        std::string user;
        std::string password;
        std::string from_address;
        bool use_tls = true;

        [[nodiscard]] bool isValid() const;
        [[nodiscard]] bool isMock() const;
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

        const MbSmtpConfig &config() const;

        bool isConfigured() const;

        bool send(const MbEmailMsg &message);

        std::string loadTemplate(const std::string &template_name,
                                  const std::unordered_map<std::string, std::string> &variables) const;

        bool checkRateLimit(const std::string &email, int max_per_hour = 3);

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
        MbSmtpConfig m_config;
        bool m_configured = false;

        mutable std::mutex m_rate_limit_mutex;
        std::unordered_map<std::string, MbRateLimitEntry> m_rate_limits;

        std::vector<MbEmailMsg> m_sent_messages;
    };
} // namespace mb

#endif //MANTISBASE_EMAIL_H
