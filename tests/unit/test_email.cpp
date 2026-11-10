#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "mantisbase/core/email.h"
#include "mantisbase/core/exceptions.h"
#include "../common/test_fixture.h"

namespace {

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
void closeSocket(Socket fd) { closesocket(fd); }
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
void closeSocket(Socket fd) { ::close(fd); }
#endif

bool sendAll(Socket fd, const std::string &data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

std::string recvLine(Socket fd) {
    std::string line;
    char c = 0;
    while (line.size() < 100000) {
        const auto n = ::recv(fd, &c, 1, 0);
        if (n <= 0)
            break;
        line.push_back(c);
        if (c == '\n')
            break;
    }
    return line;
}

enum class SmtpMode {
    Plain,
    RejectMail,
    NoStartTls,
    StartTlsThenClose
};

class FakeSmtp {
public:
    explicit FakeSmtp(SmtpMode mode) : mode_(mode) {
        listen_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_ == kInvalidSocket)
            throw std::runtime_error("socket failed");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (::bind(listen_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
            closeSocket(listen_);
            throw std::runtime_error("bind failed");
        }
        if (::listen(listen_, 1) != 0) {
            closeSocket(listen_);
            throw std::runtime_error("listen failed");
        }
        socklen_t len = sizeof(addr);
        if (::getsockname(listen_, reinterpret_cast<sockaddr *>(&addr), &len) != 0) {
            closeSocket(listen_);
            throw std::runtime_error("getsockname failed");
        }
        port_ = ntohs(addr.sin_port);
        thread_ = std::thread([this] { run(); });
    }

    ~FakeSmtp() {
        stop_.store(true);
        if (listen_ != kInvalidSocket) {
#ifdef _WIN32
            ::shutdown(listen_, SD_BOTH);
#else
            ::shutdown(listen_, SHUT_RDWR);
#endif
        }
        if (thread_.joinable())
            thread_.join();
        if (listen_ != kInvalidSocket)
            closeSocket(listen_);
    }

    [[nodiscard]] int port() const { return port_; }

    [[nodiscard]] std::string transcript() const {
        std::lock_guard lock(mu_);
        return transcript_;
    }

    [[nodiscard]] std::string data() const {
        std::lock_guard lock(mu_);
        return data_;
    }

    [[nodiscard]] int firstTlsByte() const { return firstTlsByte_.load(); }

private:
    void run() {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_, &fds);
        timeval tv{};
        tv.tv_sec = 5;
        const auto ready = ::select(static_cast<int>(listen_) + 1, &fds, nullptr, nullptr, &tv);
        if (ready <= 0 || stop_.load())
            return;

        const Socket client = ::accept(listen_, nullptr, nullptr);
        if (client == kInvalidSocket)
            return;

#ifdef _WIN32
        DWORD timeoutMs = 5000;
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
#else
        timeval recvTv{};
        recvTv.tv_sec = 5;
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &recvTv, sizeof(recvTv));
#endif

        if (!sendAll(client, "220 fake.smtp ESMTP\r\n")) {
            closeSocket(client);
            return;
        }

        bool inData = false;
        bool finishedData = false;
        while (!stop_.load()) {
            if (mode_ == SmtpMode::StartTlsThenClose && sawStartTls_) {
                char byte = 0;
                const auto n = ::recv(client, &byte, 1, 0);
                if (n == 1)
                    firstTlsByte_.store(static_cast<unsigned char>(byte));
                break;
            }

            const auto line = recvLine(client);
            if (line.empty())
                break;
            finishedData = false;
            {
                std::lock_guard lock(mu_);
                transcript_ += line;
                if (inData) {
                    if (line == ".\r\n" || line == ".\n") {
                        inData = false;
                        finishedData = true;
                    } else {
                        data_ += line;
                    }
                }
            }
            if (finishedData) {
                if (!sendAll(client, "250 Queued\r\n"))
                    break;
                continue;
            }
            if (inData)
                continue;

            if (line.rfind("EHLO", 0) == 0 || line.rfind("HELO", 0) == 0) {
                if (mode_ == SmtpMode::StartTlsThenClose || mode_ == SmtpMode::NoStartTls) {
                    std::string caps = "250-fake.smtp\r\n";
                    if (mode_ == SmtpMode::StartTlsThenClose)
                        caps += "250-STARTTLS\r\n";
                    caps += "250 AUTH PLAIN LOGIN\r\n";
                    if (!sendAll(client, caps))
                        break;
                } else {
                    if (!sendAll(client, "250-fake.smtp\r\n250 AUTH PLAIN LOGIN\r\n"))
                        break;
                }
            } else if (line.rfind("STARTTLS", 0) == 0) {
                sawStartTls_ = true;
                if (!sendAll(client, "220 Ready to start TLS\r\n"))
                    break;
            } else if (line.rfind("AUTH", 0) == 0) {
                if (!sendAll(client, "235 Authentication successful\r\n"))
                    break;
            } else if (line.rfind("MAIL FROM", 0) == 0) {
                const char *reply = mode_ == SmtpMode::RejectMail
                                        ? "550 mailbox unavailable\r\n"
                                        : "250 OK\r\n";
                if (!sendAll(client, reply))
                    break;
            } else if (line.rfind("RCPT TO", 0) == 0) {
                if (!sendAll(client, "250 OK\r\n"))
                    break;
            } else if (line.rfind("DATA", 0) == 0) {
                inData = true;
                if (!sendAll(client, "354 End data with <CR><LF>.<CR><LF>\r\n"))
                    break;
            } else if (line.rfind("QUIT", 0) == 0) {
                sendAll(client, "221 Bye\r\n");
                break;
            } else if (!sendAll(client, "500 Unknown\r\n")) {
                break;
            }
        }
        closeSocket(client);
    }

    SmtpMode mode_;
    Socket listen_ = kInvalidSocket;
    int port_ = 0;
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<int> firstTlsByte_{-1};
    bool sawStartTls_ = false;
    mutable std::mutex mu_;
    std::string transcript_;
    std::string data_;
};

mb::MbEmailMsg sampleMessage() {
    return mb::MbEmailMsg{
        .to = "user@example.com",
        .subject = "Hello",
        .body_html = "",
        .body_text = "Hello there"
    };
}

} // namespace

class EmailTest : public MbAppFixture {
protected:
    void SetUp() override {
        MbAppFixture::SetUp();
        mantis().mailer().applySettings(mb::json::object());
        mantis().mailer().clearRateLimits();
        mantis().mailer().clearSentMessages();
    }

    void TearDown() override {
        mantis().mailer().applySettings(mb::json::object());
        mantis().mailer().clearRateLimits();
        mantis().mailer().clearSentMessages();
        MbAppFixture::TearDown();
    }
};

TEST_F(EmailTest, FromJsonMapsSettingsFields) {
    const auto cfg = mb::MbSmtpConfig::fromJson({
        {"host", "smtp.example.com"},
        {"port", 2525},
        {"user", "mailer"},
        {"password", "secret"},
        {"from", "App <app@example.com>"},
        {"tls", false}
    });
    EXPECT_EQ(cfg.host, "smtp.example.com");
    EXPECT_EQ(cfg.port, 2525);
    EXPECT_EQ(cfg.user, "mailer");
    EXPECT_EQ(cfg.password, "secret");
    EXPECT_EQ(cfg.from_address, "App <app@example.com>");
    EXPECT_FALSE(cfg.use_tls);
    EXPECT_TRUE(cfg.isValid());
}

TEST_F(EmailTest, ApplySettingsLeavesMailerUnconfiguredWithoutHost) {
    mantis().mailer().applySettings({{"smtp", {{"host", ""}, {"from", "app@example.com"}}}});
    EXPECT_FALSE(mantis().mailer().isConfigured());
}

TEST_F(EmailTest, SendTestRequiresConfiguration) {
    try {
        mantis().mailer().sendTest("user@example.com");
        FAIL() << "expected unconfigured mailer to reject the test message";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 400);
        EXPECT_NE(std::string(ex.what()).find("not configured"), std::string::npos);
    }
}

TEST_F(EmailTest, SendTestRejectsAddressWithoutAt) {
    mb::MbSmtpConfig cfg;
    cfg.host = "mock";
    cfg.from_address = "app@example.com";
    mantis().mailer().configure(cfg);

    try {
        mantis().mailer().sendTest("not-an-email");
        FAIL() << "expected an address without @ to be rejected";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 400);
    }
    EXPECT_TRUE(mantis().mailer().sentMessages().empty());
}

TEST_F(EmailTest, SendTestCapturesFixedMessage) {
    mb::MbSmtpConfig cfg;
    cfg.host = "mock";
    cfg.port = 587;
    cfg.from_address = "app@example.com";
    cfg.use_tls = false;
    mantis().mailer().configure(cfg);

    EXPECT_TRUE(mantis().mailer().sendTest("  user@example.com  "));
    ASSERT_EQ(mantis().mailer().sentMessages().size(), 1u);
    const auto &sent = mantis().mailer().sentMessages().front();
    EXPECT_EQ(sent.to, "user@example.com");
    EXPECT_EQ(sent.subject, "MantisBase: Hey, it works!");
    EXPECT_EQ(sent.body_text, "If you are seeing this, the SMTP Settings are OK!");
}

TEST_F(EmailTest, MockTransportCapturesMessage) {
    mb::MbSmtpConfig cfg;
    cfg.host = "mock";
    cfg.port = 587;
    cfg.from_address = "app@example.com";
    cfg.use_tls = false;
    mantis().mailer().configure(cfg);

    EXPECT_TRUE(mantis().mailer().send(sampleMessage()));
    ASSERT_EQ(mantis().mailer().sentMessages().size(), 1u);
    EXPECT_EQ(mantis().mailer().sentMessages().front().subject, "Hello");
}

TEST_F(EmailTest, RejectsHeaderInjection) {
    mb::MbSmtpConfig cfg;
    cfg.host = "mock";
    cfg.from_address = "app@example.com";
    mantis().mailer().configure(cfg);

    auto msg = sampleMessage();
    msg.subject = "Hello\r\nBcc: victim@example.com";
    try {
        mantis().mailer().send(msg);
        FAIL() << "expected header injection to be rejected";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 400);
    }
    EXPECT_TRUE(mantis().mailer().sentMessages().empty());
}

TEST_F(EmailTest, RateLimitBlocksFourthMessage) {
    mb::MbSmtpConfig cfg;
    cfg.host = "mock";
    cfg.from_address = "app@example.com";
    mantis().mailer().configure(cfg);

    for (int i = 0; i < 3; ++i)
        EXPECT_TRUE(mantis().mailer().send(sampleMessage()));

    try {
        mantis().mailer().send(sampleMessage());
        FAIL() << "expected rate limit";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 429);
    }
}

TEST_F(EmailTest, PlainSmtpDeliversMessage) {
    FakeSmtp server(SmtpMode::Plain);
    mb::MbSmtpConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = server.port();
    cfg.user = "mailer";
    cfg.password = "secret";
    cfg.from_address = "App <app@example.com>";
    cfg.use_tls = false;
    mantis().mailer().configure(cfg);

    auto msg = sampleMessage();
    msg.body_text = ".\nHello there";
    EXPECT_TRUE(mantis().mailer().send(msg));

    const auto transcript = server.transcript();
    EXPECT_NE(transcript.find("EHLO"), std::string::npos);
    EXPECT_NE(transcript.find("AUTH PLAIN"), std::string::npos);
    EXPECT_NE(transcript.find("MAIL FROM:<app@example.com>"), std::string::npos);
    EXPECT_NE(transcript.find("RCPT TO:<user@example.com>"), std::string::npos);
    EXPECT_NE(server.data().find(".."), std::string::npos);
    EXPECT_NE(server.data().find("Hello there"), std::string::npos);
    EXPECT_TRUE(mantis().mailer().sentMessages().empty());
}

TEST_F(EmailTest, MailFromRejectionSurfaces) {
    FakeSmtp server(SmtpMode::RejectMail);
    mb::MbSmtpConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = server.port();
    cfg.from_address = "app@example.com";
    cfg.use_tls = false;
    mantis().mailer().configure(cfg);

    try {
        mantis().mailer().send(sampleMessage());
        FAIL() << "expected SMTP rejection";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 502);
        EXPECT_NE(std::string(ex.what()).find("MAIL FROM"), std::string::npos);
    }
}

TEST_F(EmailTest, MissingStartTlsIsRejectedBeforeAuth) {
    FakeSmtp server(SmtpMode::NoStartTls);
    mb::MbSmtpConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = server.port();
    cfg.user = "mailer";
    cfg.password = "secret";
    cfg.from_address = "app@example.com";
    cfg.use_tls = true;
    mantis().mailer().configure(cfg);

    try {
        mantis().mailer().send(sampleMessage());
        FAIL() << "expected STARTTLS requirement";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 502);
        EXPECT_NE(std::string(ex.what()).find("STARTTLS"), std::string::npos);
    }
    EXPECT_EQ(server.transcript().find("AUTH"), std::string::npos);
}

TEST_F(EmailTest, StartTlsSendsClientHello) {
    FakeSmtp server(SmtpMode::StartTlsThenClose);
    mb::MbSmtpConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = server.port();
    cfg.from_address = "app@example.com";
    cfg.use_tls = true;
    mantis().mailer().configure(cfg);

    try {
        mantis().mailer().send(sampleMessage());
        FAIL() << "expected TLS handshake to fail against the fake server";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 502);
    }
    EXPECT_EQ(server.firstTlsByte(), 0x16);
    EXPECT_EQ(server.transcript().find("AUTH"), std::string::npos);
}

TEST_F(EmailTest, ConnectionRefusedFailsFast) {
    mb::MbSmtpConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 1;
    cfg.from_address = "app@example.com";
    cfg.use_tls = false;
    mantis().mailer().configure(cfg);

    const auto started = std::chrono::steady_clock::now();
    try {
        mantis().mailer().send(sampleMessage());
        FAIL() << "expected connection failure";
    } catch (const mb::MbException &ex) {
        EXPECT_EQ(ex.code(), 502);
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    EXPECT_LT(elapsed, std::chrono::seconds(5));
}
