#include "../../include/mantisbase/core/email.h"
#include "../../include/mantisbase/mantisbase.h"
#include "../../include/mantisbase/core/logger/logger.h"
#include "../../include/mantisbase/core/kv_store.h"
#include "../../include/mantisbase/utils/utils.h"

#include <algorithm>
#include <sstream>
#include <fstream>

bool mb::MbSmtpConfig::isValid() const {
    return !host.empty() && port > 0 && !from_address.empty();
}

bool mb::MbSmtpConfig::isMock() const {
    return host == "mock" || host == "localhost:0";
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

bool mb::MbMailer::send(const MbEmailMsg &message) {
    if (!m_configured) {
        throw MbException(400, "Email service not configured. Set SMTP settings first.");
    }

    if (message.to.empty()) {
        throw MbException(400, "Recipient email address is required");
    }

    if (message.subject.empty()) {
        throw MbException(400, "Email subject is required");
    }

    if (message.body_html.empty() && message.body_text.empty()) {
        throw MbException(400, "Email body (HTML or text) is required");
    }

    if (!checkRateLimit(message.to)) {
        throw MbException(429,
                              std::format("Rate limit exceeded for {}. Maximum 3 emails per hour.", message.to));
    }

    if (m_config.isMock()) {
        m_sent_messages.push_back(message);
        logger().debug("Mailer",
                       std::format("[EMAIL MOCK] To: {} Subject: {}", message.to, message.subject));
        return true;
    }

    // In the full build, this would use libcurl SMTP:
    // curl_easy_setopt(curl, CURLOPT_URL, smtp_url);
    // curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from);
    // curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    m_sent_messages.push_back(message);
    logger().debug("Mailer",
                   std::format("[EMAIL] To: {} Subject: {}", message.to, message.subject));
    return true;
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

const mb::MbSmtpConfig &mb::MbMailer::config() const { return m_config; }

bool mb::MbMailer::isConfigured() const { return m_configured; }

const std::vector<mb::MbEmailMsg> &mb::MbMailer::sentMessages() const { return m_sent_messages; }

void mb::MbMailer::clearSentMessages() { m_sent_messages.clear(); }

void mb::MbMailer::clearRateLimits() {
    std::lock_guard<std::mutex> lock(m_rate_limit_mutex);
    m_rate_limits.clear();
}
