#pragma once

#include "mantisbase/core/email.h"

#include <string>

namespace mb::smtp_detail {

/**
 * Deliver one RFC 5322 message over SMTP.
 * The TCP connection is a trantor TcpClient. TLS (implicit on port 465,
 * otherwise STARTTLS) is performed with wolfSSL because trantor is built
 * without its own TLS provider.
 *
 * @param mailFrom Envelope sender, without angle brackets.
 * @param rcptTo Envelope recipient, without angle brackets.
 * @param payload Bytes to send after the server accepts DATA, including the
 *        terminating `.\r\n`.
 * @throws MbException 502 on DNS, connection, TLS, or protocol failure.
 */
void deliver(const MbSmtpConfig &config,
             const std::string &mailFrom,
             const std::string &rcptTo,
             std::string payload);

} // namespace mb::smtp_detail
