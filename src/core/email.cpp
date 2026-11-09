#include "../../include/mantisbase/core/email.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/logger/logger.h"
#include "../../include/mantisbase/core/kv_store.h"
#include "../../include/mantisbase/core/exceptions.h"
#include "../../include/mantisbase/utils/utils.h"
#include "../../include/mantisbase/core/smtp_client.h"

#include <wolfssl/ssl.h>

#include <trantor/net/EventLoopThread.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/TcpClient.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

bool mb::MbSmtpConfig::isValid() const {
    return !host.empty() && port > 0 && port <= 65535 && !from_address.empty();
}

bool mb::MbSmtpConfig::isMock() const {
    return host == "mock" || host == "localhost:0";
}

mb::MbSmtpConfig mb::MbSmtpConfig::fromJson(const json &smtp) {
    MbSmtpConfig cfg;
    if (smtp.contains("host") && smtp["host"].is_string())
        cfg.host = trim(smtp["host"].get<std::string>());
    if (smtp.contains("port") && smtp["port"].is_number_integer())
        cfg.port = smtp["port"].get<int>();
    if (smtp.contains("user") && smtp["user"].is_string())
        cfg.user = smtp["user"].get<std::string>();
    if (smtp.contains("password") && smtp["password"].is_string())
        cfg.password = smtp["password"].get<std::string>();
    if (smtp.contains("from") && smtp["from"].is_string())
        cfg.from_address = trim(smtp["from"].get<std::string>());
    else if (smtp.contains("from_address") && smtp["from_address"].is_string())
        cfg.from_address = trim(smtp["from_address"].get<std::string>());
    if (smtp.contains("tls") && smtp["tls"].is_boolean())
        cfg.use_tls = smtp["tls"].get<bool>();
    else if (smtp.contains("use_tls") && smtp["use_tls"].is_boolean())
        cfg.use_tls = smtp["use_tls"].get<bool>();
    return cfg;
}

mb::MbMailer::MbMailer(const MantisBase &app) : MbAppMixin(app) {
}

void mb::MbMailer::configure(const MbSmtpConfig &config) {
    if (!config.isValid()) {
        throw MbException(400, "Invalid SMTP configuration: host and from_address are required");
    }
    m_config = config;
    m_configured = true;
}

void mb::MbMailer::applySettings(const json &configs) {
    if (!configs.contains("smtp") || !configs["smtp"].is_object()) {
        if (m_configured)
            logger().info("Mailer", "SMTP disabled", "");
        m_configured = false;
        m_config = {};
        return;
    }

    const auto cfg = MbSmtpConfig::fromJson(configs["smtp"]);
    if (!cfg.isValid()) {
        const bool wasConfigured = m_configured;
        m_configured = false;
        m_config = cfg;
        if (!cfg.host.empty()) {
            logger().warn("Mailer", "SMTP settings ignored", "host and from are required");
        } else if (wasConfigured) {
            logger().info("Mailer", "SMTP disabled", "");
        }
        return;
    }

    m_config = cfg;
    m_configured = true;
    logger().info("Mailer", "SMTP configured",
                  std::format("{}:{} tls={}", cfg.host, cfg.port, cfg.use_tls ? "on" : "off"));
}

namespace {
    bool unsafeHeader(const std::string &value) {
        return value.find('\r') != std::string::npos ||
               value.find('\n') != std::string::npos ||
               value.find('\0') != std::string::npos;
    }

    void rejectUnsafeHeader(const std::string &label, const std::string &value) {
        if (unsafeHeader(value))
            throw mb::MbException(400, label + " contains an invalid character");
    }

    std::string htmlEscape(const std::string &value) {
        std::string out;
        out.reserve(value.size());
        for (const unsigned char c : value) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                case '\'': out += "&#39;"; break;
                default: out.push_back(static_cast<char>(c)); break;
            }
        }
        return out;
    }

    std::string envelopeAddress(const std::string &raw) {
        std::string addr = raw;
        const auto lt = raw.find('<');
        const auto gt = raw.rfind('>');
        if (lt != std::string::npos && gt != std::string::npos && gt > lt)
            addr = raw.substr(lt + 1, gt - lt - 1);
        if (addr.empty() || addr.find_first_of(" \t\r\n<>") != std::string::npos)
            throw mb::MbException(400, "Invalid email address");
        return addr;
    }

    std::string toCrlf(const std::string &input) {
        std::string out;
        out.reserve(input.size());
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\r') {
                out += "\r\n";
                if (i + 1 < input.size() && input[i + 1] == '\n')
                    ++i;
            } else if (input[i] == '\n') {
                out += "\r\n";
            } else {
                out.push_back(input[i]);
            }
        }
        return out;
    }

    std::string dotStuff(const std::string &crlf) {
        std::string out;
        out.reserve(crlf.size() + 8);
        if (!crlf.empty() && crlf.front() == '.')
            out.push_back('.');
        for (std::size_t i = 0; i < crlf.size(); ++i) {
            out.push_back(crlf[i]);
            if (crlf[i] == '\n' && i + 1 < crlf.size() && crlf[i + 1] == '.')
                out.push_back('.');
        }
        if (!out.ends_with("\r\n"))
            out += "\r\n";
        return out;
    }

    std::string rfc5322Date() {
        const std::time_t now = std::time(nullptr);
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &now);
#else
        gmtime_r(&now, &tm);
#endif
        char buf[64] = {};
        if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S +0000", &tm) == 0)
            return "Thu, 01 Jan 1970 00:00:00 +0000";
        return buf;
    }

    std::string base64Encode(const std::string &input) {
        static constexpr char kTable[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((input.size() + 2) / 3) * 4);
        std::size_t i = 0;
        while (i + 2 < input.size()) {
            const unsigned n = (static_cast<unsigned char>(input[i]) << 16) |
                               (static_cast<unsigned char>(input[i + 1]) << 8) |
                               static_cast<unsigned char>(input[i + 2]);
            out.push_back(kTable[(n >> 18) & 63]);
            out.push_back(kTable[(n >> 12) & 63]);
            out.push_back(kTable[(n >> 6) & 63]);
            out.push_back(kTable[n & 63]);
            i += 3;
        }
        if (i < input.size()) {
            unsigned n = static_cast<unsigned char>(input[i]) << 16;
            out.push_back(kTable[(n >> 18) & 63]);
            if (i + 1 < input.size()) {
                n |= static_cast<unsigned char>(input[i + 1]) << 8;
                out.push_back(kTable[(n >> 12) & 63]);
                out.push_back(kTable[(n >> 6) & 63]);
                out.push_back('=');
            } else {
                out.push_back(kTable[(n >> 12) & 63]);
                out.push_back('=');
                out.push_back('=');
            }
        }
        return out;
    }

    std::string encodeHeader(const std::string &value) {
        const bool ascii = std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return c >= 32 && c <= 126;
        });
        if (ascii)
            return value;
        return "=?UTF-8?B?" + base64Encode(value) + "?=";
    }

    std::string addressHeader(const std::string &value) {
        const std::string encoded = encodeHeader(value);
        if (value.find('<') != std::string::npos)
            return encoded;
        return "<" + encoded + ">";
    }

    std::string buildRfc5322(const mb::MbEmailMsg &message, const std::string &from) {
        const std::string boundary = "mb-" + mb::generateShortId(24);
        std::ostringstream msg;
        msg << "Date: " << rfc5322Date() << "\r\n"
                << "From: " << addressHeader(from) << "\r\n"
                << "To: " << addressHeader(message.to) << "\r\n"
                << "Subject: " << encodeHeader(message.subject) << "\r\n"
                << "Message-ID: <" << mb::generateShortId(16) << "@mantisbase.local>\r\n"
                << "MIME-Version: 1.0\r\n";

        const bool hasText = !message.body_text.empty();
        const bool hasHtml = !message.body_html.empty();
        if (hasText && hasHtml) {
            msg << "Content-Type: multipart/alternative; boundary=\"" << boundary << "\"\r\n"
                    << "\r\n"
                    << "--" << boundary << "\r\n"
                    << "Content-Type: text/plain; charset=utf-8\r\n"
                    << "Content-Transfer-Encoding: 8bit\r\n"
                    << "\r\n"
                    << dotStuff(toCrlf(message.body_text))
                    << "--" << boundary << "\r\n"
                    << "Content-Type: text/html; charset=utf-8\r\n"
                    << "Content-Transfer-Encoding: 8bit\r\n"
                    << "\r\n"
                    << dotStuff(toCrlf(message.body_html))
                    << "--" << boundary << "--\r\n";
        } else if (hasHtml) {
            msg << "Content-Type: text/html; charset=utf-8\r\n"
                    << "Content-Transfer-Encoding: 8bit\r\n"
                    << "\r\n"
                    << dotStuff(toCrlf(message.body_html));
        } else {
            msg << "Content-Type: text/plain; charset=utf-8\r\n"
                    << "Content-Transfer-Encoding: 8bit\r\n"
                    << "\r\n"
                    << dotStuff(toCrlf(message.body_text));
        }

        std::string payload = msg.str();
        if (!payload.ends_with("\r\n"))
            payload += "\r\n";
        payload += ".\r\n";
        return payload;
    }
} // namespace

bool mb::MbMailer::send(const MbEmailMsg &message) {
    if (!m_configured)
        throw MbException(400, std::string(kNotConfiguredMessage));
    if (message.to.empty())
        throw MbException(400, "Recipient email address is required");
    if (message.subject.empty())
        throw MbException(400, "Email subject is required");
    if (message.body_html.empty() && message.body_text.empty())
        throw MbException(400, "Email body (HTML or text) is required");

    rejectUnsafeHeader("Recipient", message.to);
    rejectUnsafeHeader("Subject", message.subject);
    rejectUnsafeHeader("From", m_config.from_address);

    const auto mailFrom = envelopeAddress(m_config.from_address);
    const auto rcptTo = envelopeAddress(message.to);
    auto payload = buildRfc5322(message, m_config.from_address);

    if (!checkRateLimit(message.to)) {
        throw MbException(429, std::format("Rate limit exceeded for {}. Maximum 3 emails per hour.", message.to));
    }

    if (m_config.isMock()) {
        m_sent_messages.push_back(message);
        logger().debug("Mailer", std::format("[EMAIL MOCK] To: {} Subject: {}", message.to, message.subject));
        return true;
    }

    try {
        mb::smtp_detail::deliver(m_config, mailFrom, rcptTo, std::move(payload));
    } catch (const MbException &e) {
        logger().warn("Mailer", "Email send failed",
                      std::format("To: {} via {}:{}: {}", message.to, m_config.host, m_config.port, e.what()));
        throw;
    }

    logger().debug("Mailer", "Email sent",
                   std::format("\n\tTo: {} \n\tSubject: {}",
                               hideEmail(message.to), message.subject));
    return true;
}

bool mb::MbMailer::sendTest(const std::string &to) {
    const auto recipient = trim(to);
    if (recipient.empty() || recipient.front() == '@' || recipient.back() == '@'
        || recipient.find('@') == std::string::npos)
        throw MbException(400, "Invalid email address");

    MbEmailMsg message;
    message.to = recipient;
    message.subject = std::string(kTestMessage);
    message.body_text = "If you are seeing this, the SMTP Settings are OK!";
    return send(message);
}

std::string mb::MbMailer::loadTemplate(const std::string &template_name,
                                       const std::unordered_map<std::string, std::string> &variables) const {
    std::string content;
    if (!mbApp().templatesDir().empty()) {
        std::string path = (std::filesystem::path(mbApp().templatesDir()) / template_name).string();
        if (std::ifstream file(path); file.is_open()) {
            std::ostringstream ss;
            ss << file.rdbuf();
            content = ss.str();
        }
    }

    if (content.empty()) {
        if (template_name == "verification.html") {
            auto url_it = variables.find("verify_url");
            auto app_it = variables.find("app_name");
            content = verificationEmailHtml(
                app_it != variables.end() ? app_it->second : "MantisBase",
                url_it != variables.end() ? url_it->second : "#");
        } else if (template_name == "verification.txt") {
            auto url_it = variables.find("verify_url");
            auto app_it = variables.find("app_name");
            content = verificationEmailText(
                app_it != variables.end() ? app_it->second : "MantisBase",
                url_it != variables.end() ? url_it->second : "#");
        } else if (template_name == "password_reset.html") {
            auto url_it = variables.find("reset_url");
            auto app_it = variables.find("app_name");
            content = passwordResetEmailHtml(
                app_it != variables.end() ? app_it->second : "MantisBase",
                url_it != variables.end() ? url_it->second : "#");
        } else if (template_name == "password_reset.txt") {
            auto url_it = variables.find("reset_url");
            auto app_it = variables.find("app_name");
            content = passwordResetEmailText(
                app_it != variables.end() ? app_it->second : "MantisBase",
                url_it != variables.end() ? url_it->second : "#");
        } else if (template_name == "login_otp.html" || template_name == "login_otp.txt") {
            auto app_it = variables.find("app_name");
            auto otp_it = variables.find("otp");
            auto exp_it = variables.find("expires_minutes");
            int minutes = 10;
            if (exp_it != variables.end()) {
                try {
                    minutes = std::stoi(exp_it->second);
                } catch (const std::exception &) {
                    minutes = 10;
                }
            }
            const auto app_name = app_it != variables.end() ? app_it->second : "MantisBase";
            const auto code = otp_it != variables.end() ? otp_it->second : "";
            content = template_name == "login_otp.html"
                          ? loginOtpEmailHtml(app_name, code, minutes)
                          : loginOtpEmailText(app_name, code, minutes);
        } else {
            throw MbException(404, "Unknown template: " + template_name);
        }
        return content;
    }

    for (const auto &[key, value]: variables) {
        std::string placeholder = "{{" + key + "}}";
        size_t pos = 0;
        while ((pos = content.find(placeholder, pos)) != std::string::npos) {
            content.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }

    return content;
}

bool mb::MbMailer::checkRateLimit(const std::string &email, const int max_per_hour) {
    std::lock_guard<std::mutex> lock(m_rate_limit_mutex);

    const auto now = std::chrono::steady_clock::now();
    auto one_hour_ago = now - std::chrono::hours(1);

    auto &entry = m_rate_limits[email];
    entry.email = email;

    entry.timestamps.erase(
        std::ranges::remove_if(entry.timestamps,
                               [&one_hour_ago](const auto &ts) { return ts < one_hour_ago; }).begin(),
        entry.timestamps.end());

    if (static_cast<int>(entry.timestamps.size()) >= max_per_hour) {
        return false;
    }

    entry.timestamps.push_back(now);
    return true;
}

std::string mb::MbMailer::verificationEmailHtml(const std::string &app_name, const std::string &verify_url) {
    std::ostringstream html;
    html << "<!DOCTYPE html>\n"
            << "<html><head><meta charset=\"utf-8\"></head>\n"
            << "<body style=\"font-family: -apple-system, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px;\">\n"
            << "<h2>Verify your email for " << app_name << "</h2>\n"
            << "<p>Click the button below to verify your email address:</p>\n"
            << "<p><a href=\"" << verify_url << "\" style=\"display: inline-block; padding: 12px 24px; "
            << "background-color: #4F46E5; color: white; text-decoration: none; border-radius: 6px;\">"
            << "Verify Email</a></p>\n"
            << "<p style=\"color: #6B7280; font-size: 14px;\">This link expires in 1 hour. "
            << "If you didn't create an account, you can safely ignore this email.</p>\n"
            << "</body></html>";
    return html.str();
}

std::string mb::MbMailer::verificationEmailText(const std::string &app_name, const std::string &verify_url) {
    std::ostringstream text;
    text << "Verify your email for " << app_name << "\n\n"
            << "Visit the following link to verify your email address:\n\n"
            << verify_url << "\n\n"
            << "This link expires in 1 hour.\n"
            << "If you didn't create an account, you can safely ignore this email.\n";
    return text.str();
}

std::string mb::MbMailer::passwordResetEmailHtml(const std::string &app_name, const std::string &reset_url) {
    std::ostringstream html;
    html << "<!DOCTYPE html>\n"
            << "<html><head><meta charset=\"utf-8\"></head>\n"
            << "<body style=\"font-family: -apple-system, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px;\">\n"
            << "<h2>Reset your password for " << app_name << "</h2>\n"
            << "<p>Click the button below to reset your password:</p>\n"
            << "<p><a href=\"" << reset_url << "\" style=\"display: inline-block; padding: 12px 24px; "
            << "background-color: #DC2626; color: white; text-decoration: none; border-radius: 6px;\">"
            << "Reset Password</a></p>\n"
            << "<p style=\"color: #6B7280; font-size: 14px;\">This link expires in 1 hour. "
            << "If you didn't request a password reset, you can safely ignore this email.</p>\n"
            << "</body></html>";
    return html.str();
}

std::string mb::MbMailer::passwordResetEmailText(const std::string &app_name, const std::string &reset_url) {
    std::ostringstream text;
    text << "Reset your password for " << app_name << "\n\n"
            << "Visit the following link to reset your password:\n\n"
            << reset_url << "\n\n"
            << "This link expires in 1 hour.\n"
            << "If you didn't request a password reset, you can safely ignore this email.\n";
    return text.str();
}

std::string mb::MbMailer::loginOtpEmailHtml(const std::string &app_name,
                                              const std::string &code,
                                              const int expires_minutes) {
    const auto safe_app = htmlEscape(app_name);
    const auto safe_code = htmlEscape(code);
    std::ostringstream html;
    html << "<!DOCTYPE html>\n"
            << "<html><head><meta charset=\"utf-8\"></head>\n"
            << "<body style=\"font-family: -apple-system, sans-serif; max-width: 600px; margin: 0 auto; padding: 20px;\">\n"
            << "<h2>Sign in to " << safe_app << "</h2>\n"
            << "<p>Enter this code to sign in. It expires in " << expires_minutes << " minutes.</p>\n"
            << "<p style=\"font-size: 32px; font-weight: 700; letter-spacing: 0.3em;\">" << safe_code << "</p>\n"
            << "<p style=\"color: #6B7280; font-size: 14px;\">If you didn't try to sign in, you can ignore this email.</p>\n"
            << "</body></html>";
    return html.str();
}

std::string mb::MbMailer::loginOtpEmailText(const std::string &app_name,
                                              const std::string &code,
                                              const int expires_minutes) {
    std::ostringstream text;
    text << "Sign in to " << app_name << "\n\n"
            << "Your sign-in code is:\n\n"
            << code << "\n\n"
            << "This code expires in " << expires_minutes << " minutes.\n"
            << "If you didn't try to sign in, you can ignore this email.\n";
    return text.str();
}

std::string mb::MbMailer::hideEmail(const std::string &email) {
    const auto at = email.find('@');

    if (at == std::string::npos || at == 0) {
        return "***";
    }

    const std::string username = email.substr(0, at);
    const std::string domain = email.substr(at);

    if (username.size() <= 2) {
        return username[0] + std::string(username.size() - 1, '*') + domain;
    }

    return username.substr(0, 2) +
           std::string(username.size() - 2, '*') +
           domain;
}

const mb::MbSmtpConfig &mb::MbMailer::config() const { return m_config; }

bool mb::MbMailer::isConfigured() const { return m_configured; }

const std::vector<mb::MbEmailMsg> &mb::MbMailer::sentMessages() const { return m_sent_messages; }

void mb::MbMailer::clearSentMessages() { m_sent_messages.clear(); }

void mb::MbMailer::clearRateLimits() {
    std::lock_guard<std::mutex> lock(m_rate_limit_mutex);
    m_rate_limits.clear();
}
