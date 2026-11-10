#include "../../include/mantisbase/core/smtp_client.h"

#include "mantisbase/core/exceptions.h"
#include "mantisbase/utils/utils.h"

#include <wolfssl/ssl.h>

#include <trantor/net/EventLoopThread.h>
#include <trantor/net/InetAddress.h>
#include <trantor/net/TcpClient.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace mb::smtp_detail {
    namespace {
        constexpr int kImplicitTlsPort = 465;
        constexpr double kTimeoutSec = 20.0;
        constexpr std::size_t kMaxBuffer = 1024 * 1024;

        struct DeliveryResult {
            bool ok = false;
            std::string error;
        };

        struct TlsEnv {
            WOLFSSL_CTX *ctx = nullptr;
            std::string error;
        };

        std::string tlsErrorString(int err) {
            char buf[160] = {};
            wolfSSL_ERR_error_string_n(static_cast<unsigned long>(err), buf, sizeof(buf));
            if (buf[0] == '\0')
                return "error " + std::to_string(err);
            return std::string(buf) + " (" + std::to_string(err) + ")";
        }

        bool loadCaFile(WOLFSSL_CTX *ctx, const char *path) {
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
                return false;
            return wolfSSL_CTX_load_verify_locations(ctx, path, nullptr) == WOLFSSL_SUCCESS;
        }

        const TlsEnv &tlsEnv() {
            static TlsEnv env;
            static std::once_flag once;
            std::call_once(once, [] {
                if (wolfSSL_Init() != WOLFSSL_SUCCESS) {
                    env.error = "wolfSSL_Init failed";
                    return;
                }

                env.ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
                if (env.ctx == nullptr) {
                    env.error = "Failed to create SMTP TLS context";
                    return;
                }

                wolfSSL_CTX_SetMinVersion(env.ctx, WOLFSSL_TLSV1_2);
                wolfSSL_CTX_set_verify(env.ctx, WOLFSSL_VERIFY_PEER, nullptr);

                bool loaded = false;
#ifdef WOLFSSL_SYS_CA_CERTS
                if (wolfSSL_CTX_load_system_CA_certs(env.ctx) == WOLFSSL_SUCCESS)
                    loaded = true;
#endif
                static constexpr const char *kCaFiles[] = {
                    "/etc/ssl/certs/ca-certificates.crt",
                    "/etc/pki/tls/certs/ca-bundle.crt",
                    "/etc/ssl/ca-bundle.pem",
                    "/etc/ssl/cert.pem",
                    "/etc/ssl/certs/ca-bundle.crt",
                    "/opt/homebrew/etc/ca-certificates/cert.pem",
                    "/usr/local/etc/openssl@3/cert.pem",
                    "/usr/local/etc/openssl/cert.pem",
                    "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
                };
                if (!loaded) {
                    for (const char *path : kCaFiles) {
                        if (loadCaFile(env.ctx, path)) {
                            loaded = true;
                            break;
                        }
                    }
                }

                if (!loaded) {
                    wolfSSL_CTX_free(env.ctx);
                    env.ctx = nullptr;
                    env.error = "SMTP TLS is unavailable: no CA certificates could be loaded";
                }
            });
            return env;
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

        std::string ehloName() {
            char host[256] = {};
            if (::gethostname(host, sizeof(host) - 1) != 0)
                return "mantisbase";
            std::string name = host;
            if (name.empty())
                return "mantisbase";
            for (const unsigned char c : name) {
                if (c <= 32 || c >= 127)
                    return "mantisbase";
            }
            return name;
        }

        bool isIpLiteral(const std::string &host) {
            in_addr v4{};
            in6_addr v6{};
            return ::inet_pton(AF_INET, host.c_str(), &v4) == 1 ||
                   ::inet_pton(AF_INET6, host.c_str(), &v6) == 1;
        }

        std::optional<trantor::InetAddress> resolveHost(const std::string &host, int port, std::string &error) {
            addrinfo hints{};
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_family = AF_UNSPEC;
            addrinfo *result = nullptr;
            const auto service = std::to_string(port);
            const int rc = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
            if (rc != 0) {
                error = std::string("DNS lookup failed: ") + gai_strerror(rc);
                return std::nullopt;
            }

            std::optional<trantor::InetAddress> address;
            for (auto *it = result; it != nullptr; it = it->ai_next) {
                if (it->ai_family == AF_INET && it->ai_addrlen >= static_cast<socklen_t>(sizeof(sockaddr_in))) {
                    address = trantor::InetAddress(*reinterpret_cast<sockaddr_in *>(it->ai_addr));
                    break;
                }
            }
            if (!address) {
                for (auto *it = result; it != nullptr; it = it->ai_next) {
                    if (it->ai_family == AF_INET6 && it->ai_addrlen >= static_cast<socklen_t>(sizeof(sockaddr_in6))) {
                        address = trantor::InetAddress(*reinterpret_cast<sockaddr_in6 *>(it->ai_addr));
                        break;
                    }
                }
            }
            ::freeaddrinfo(result);
            if (!address)
                error = "DNS lookup returned no usable address";
            return address;
        }

        enum class ReplyParse { NeedMore, Ok, Bad };

        struct SmtpReply {
            int code = 0;
            std::string body;

            [[nodiscard]] std::string summary() const {
                std::string text = std::to_string(code);
                if (!body.empty()) {
                    text.push_back(' ');
                    text.append(body, 0, std::min<std::size_t>(body.size(), 300));
                }
                for (char &c : text) {
                    if (c == '\r' || c == '\n')
                        c = ' ';
                }
                return text;
            }

            [[nodiscard]] bool hasCapability(const std::string &cap) const {
                std::istringstream in(body);
                std::string line;
                std::string want = cap;
                toUpperCase(want);
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    const auto sp = line.find(' ');
                    std::string token = sp == std::string::npos ? line : line.substr(0, sp);
                    toUpperCase(token);
                    if (token == want)
                        return true;
                }
                return false;
            }

            [[nodiscard]] bool hasAuthMechanism(const std::string &mech) const {
                std::istringstream in(body);
                std::string line;
                std::string want = mech;
                toUpperCase(want);
                while (std::getline(in, line)) {
                    std::string upper = line;
                    toUpperCase(upper);
                    const auto pos = upper.find("AUTH");
                    if (pos == std::string::npos)
                        continue;
                    std::string rest = upper.substr(pos + 4);
                    for (char &c : rest) {
                        if (c == '=')
                            c = ' ';
                    }
                    std::istringstream tokens(rest);
                    std::string tok;
                    while (tokens >> tok) {
                        if (tok == want)
                            return true;
                    }
                }
                return false;
            }
        };

        struct ParsedReply {
            ReplyParse status = ReplyParse::NeedMore;
            SmtpReply reply;
            std::string error;
        };

        ParsedReply takeReply(std::string &pending) {
            ParsedReply parsed;
            std::vector<std::string> lines;
            std::size_t pos = 0;
            while (lines.size() < 64) {
                const auto nl = pending.find('\n', pos);
                if (nl == std::string::npos) {
                    if (pos > 0)
                        pending.erase(0, pos);
                    return parsed;
                }

                std::string line = pending.substr(pos, nl - pos);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                pos = nl + 1;
                if (line.empty())
                    continue;

                if (line.size() < 4 ||
                    !std::isdigit(static_cast<unsigned char>(line[0])) ||
                    !std::isdigit(static_cast<unsigned char>(line[1])) ||
                    !std::isdigit(static_cast<unsigned char>(line[2])) ||
                    (line[3] != ' ' && line[3] != '-')) {
                    parsed.status = ReplyParse::Bad;
                    parsed.error = "Malformed SMTP reply";
                    return parsed;
                }

                lines.push_back(line);
                if (line[3] == ' ') {
                    pending.erase(0, pos);
                    parsed.status = ReplyParse::Ok;
                    parsed.reply.code = std::stoi(line.substr(0, 3));
                    for (const auto &part : lines) {
                        if (!parsed.reply.body.empty())
                            parsed.reply.body.push_back('\n');
                        parsed.reply.body.append(part.substr(4));
                    }
                    return parsed;
                }
            }

            parsed.status = ReplyParse::Bad;
            parsed.error = "SMTP reply is too long";
            return parsed;
        }

        class Session : public std::enable_shared_from_this<Session> {
        public:
            Session(trantor::EventLoop *loop,
                    trantor::InetAddress address,
                    MbSmtpConfig config,
                    std::string mailFrom,
                    std::string rcptTo,
                    std::string payload,
                    WOLFSSL_CTX *ctx,
                    std::shared_ptr<std::promise<DeliveryResult>> prom)
                : loop_(loop),
                  address_(std::move(address)),
                  config_(std::move(config)),
                  mailFrom_(std::move(mailFrom)),
                  rcptTo_(std::move(rcptTo)),
                  payload_(std::move(payload)),
                  ctx_(ctx),
                  prom_(std::move(prom)),
                  ehlo_(ehloName()),
                  implicit_(config_.use_tls && config_.port == kImplicitTlsPort),
                  ipLiteral_(isIpLiteral(config_.host)) {}

            void start() {
                client_ = std::make_shared<trantor::TcpClient>(loop_, address_, "mb-smtp");
                std::weak_ptr<Session> weak = shared_from_this();
                client_->setConnectionCallback([weak](const trantor::TcpConnectionPtr &conn) {
                    if (const auto self = weak.lock())
                        self->onConnection(conn);
                });
                client_->setMessageCallback([weak](const trantor::TcpConnectionPtr &, trantor::MsgBuffer *buf) {
                    if (const auto self = weak.lock())
                        self->onTcpData(buf);
                });
                client_->setConnectionErrorCallback([weak] {
                    if (const auto self = weak.lock())
                        self->fail("SMTP connection failed");
                });
                timer_ = loop_->runAfter(kTimeoutSec, [weak] {
                    if (const auto self = weak.lock())
                        self->fail("SMTP connection timed out");
                });
                client_->connect();
            }

            void fail(std::string message) { complete(DeliveryResult{false, std::move(message)}); }

            int consumeCipher(char *buf, int sz) {
                if (cipher_.empty())
                    return WOLFSSL_CBIO_ERR_WANT_READ;
                const int n = std::min(sz, static_cast<int>(cipher_.size()));
                std::memcpy(buf, cipher_.data(), static_cast<std::size_t>(n));
                cipher_.erase(0, static_cast<std::size_t>(n));
                return n;
            }

            int emitCipher(const char *buf, int sz) {
                if (!conn_ || sz < 0)
                    return WOLFSSL_CBIO_ERR_CONN_CLOSE;
                conn_->send(buf, static_cast<size_t>(sz));
                return sz;
            }

        private:
            enum class Phase {
                Greeting,
                Ehlo,
                StartTls,
                EhloSecure,
                AuthPlain,
                AuthLoginChallenge,
                AuthLoginPassword,
                MailFrom,
                RcptTo,
                Data,
                Body
            };

            void onConnection(const trantor::TcpConnectionPtr &conn) {
                if (conn->connected()) {
                    conn_ = conn;
                    conn_->setTcpNoDelay(true);
                    if (implicit_)
                        beginTls();
                    return;
                }
                if (!completed_.load()) {
                    fail(handshaking_ ? "TLS handshake failed: connection closed"
                                      : "SMTP connection closed");
                }
            }

            void onTcpData(trantor::MsgBuffer *buf) {
                if (completed_.load()) {
                    buf->retrieveAll();
                    return;
                }
                std::string bytes(buf->peek(), buf->readableBytes());
                buf->retrieveAll();
                if (bytes.empty())
                    return;

                if (!tlsActive_) {
                    if (pending_.size() + bytes.size() > kMaxBuffer) {
                        fail("SMTP reply too large");
                        return;
                    }
                    pending_.append(bytes);
                    pumpReplies();
                    return;
                }

                if (cipher_.size() + bytes.size() > kMaxBuffer) {
                    fail("SMTP TLS record too large");
                    return;
                }
                cipher_.append(bytes);
                if (!handshakeDone_)
                    driveHandshake();
                else
                    readApp();
            }

            void beginTls() {
                if (completed_.load())
                    return;
                handshaking_ = true;
                tlsActive_ = true;
                if (!pending_.empty()) {
                    cipher_.append(pending_);
                    pending_.clear();
                }
                if (ctx_ == nullptr) {
                    fail("SMTP TLS is unavailable");
                    return;
                }

                ssl_ = wolfSSL_new(ctx_);
                if (ssl_ == nullptr) {
                    fail("Failed to create SMTP TLS session");
                    return;
                }
                wolfSSL_SSLSetIORecv(ssl_, reinterpret_cast<CallbackIORecv>(&Session::ioRecv));
                wolfSSL_SSLSetIOSend(ssl_, reinterpret_cast<CallbackIOSend>(&Session::ioSend));
                wolfSSL_SetIOReadCtx(ssl_, this);
                wolfSSL_SetIOWriteCtx(ssl_, this);

                if (!ipLiteral_) {
#ifdef HAVE_SNI
                    if (config_.host.size() <= 255 &&
                        wolfSSL_UseSNI(ssl_, WOLFSSL_SNI_HOST_NAME, config_.host.c_str(),
                                       static_cast<unsigned short>(config_.host.size())) != WOLFSSL_SUCCESS) {
                        fail("Failed to set TLS server name");
                        return;
                    }
#endif
                    if (wolfSSL_check_domain_name(ssl_, config_.host.c_str()) != WOLFSSL_SUCCESS) {
                        fail("Failed to set TLS hostname check");
                        return;
                    }
                }
                driveHandshake();
            }

            void driveHandshake() {
                if (completed_.load() || ssl_ == nullptr || handshakeDone_)
                    return;
                const int ret = wolfSSL_connect(ssl_);
                if (ret == WOLFSSL_SUCCESS) {
                    handshakeDone_ = true;
                    handshaking_ = false;
                    if (!implicit_) {
                        if (!sendLine("EHLO " + ehlo_))
                            return;
                        phase_ = Phase::EhloSecure;
                    }
                    readApp();
                    return;
                }
                const int err = wolfSSL_get_error(ssl_, ret);
                if (err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE)
                    return;
                fail("TLS handshake failed: " + tlsErrorString(err));
            }

            void readApp() {
                char buf[4096];
                while (!completed_.load() && handshakeDone_ && ssl_ != nullptr) {
                    const int n = wolfSSL_read(ssl_, buf, sizeof(buf));
                    if (n > 0) {
                        pending_.append(buf, buf + n);
                        if (pending_.size() > kMaxBuffer) {
                            fail("SMTP reply too large");
                            return;
                        }
                        continue;
                    }
                    const int err = wolfSSL_get_error(ssl_, n);
                    if (err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE)
                        break;
                    if (err == WOLFSSL_ERROR_ZERO_RETURN) {
                        fail("SMTP server closed the TLS connection");
                        return;
                    }
                    fail("TLS read failed: " + tlsErrorString(err));
                    return;
                }
                if (!completed_.load())
                    pumpReplies();
            }

            void pumpReplies() {
                while (!completed_.load() && !handshaking_) {
                    auto parsed = takeReply(pending_);
                    if (parsed.status == ReplyParse::NeedMore)
                        return;
                    if (parsed.status == ReplyParse::Bad) {
                        fail(parsed.error);
                        return;
                    }
                    handleReply(parsed.reply);
                }
            }

            void handleReply(const SmtpReply &reply) {
                switch (phase_) {
                    case Phase::Greeting:
                        if (reply.code != 220) {
                            fail("SMTP greeting rejected: " + reply.summary());
                            return;
                        }
                        if (!sendLine("EHLO " + ehlo_))
                            return;
                        phase_ = Phase::Ehlo;
                        break;
                    case Phase::Ehlo:
                    case Phase::EhloSecure:
                        if (reply.code != 250) {
                            fail("EHLO rejected: " + reply.summary());
                            return;
                        }
                        caps_ = reply;
                        if (phase_ == Phase::Ehlo && config_.use_tls && !implicit_) {
                            if (!reply.hasCapability("STARTTLS")) {
                                fail("SMTP server does not support STARTTLS");
                                return;
                            }
                            if (!sendLine("STARTTLS"))
                                return;
                            phase_ = Phase::StartTls;
                            break;
                        }
                        startAuthOrMail();
                        break;
                    case Phase::StartTls:
                        if (reply.code != 220) {
                            fail("STARTTLS rejected: " + reply.summary());
                            return;
                        }
                        beginTls();
                        break;
                    case Phase::AuthPlain:
                        if (reply.code != 235) {
                            fail("SMTP authentication failed: " + reply.summary());
                            return;
                        }
                        sendMailFrom();
                        break;
                    case Phase::AuthLoginChallenge:
                        if (reply.code != 334) {
                            fail("SMTP authentication failed: " + reply.summary());
                            return;
                        }
                        if (!sendLine(base64Encode(config_.user)))
                            return;
                        phase_ = Phase::AuthLoginPassword;
                        break;
                    case Phase::AuthLoginPassword:
                        if (reply.code != 334) {
                            fail("SMTP authentication failed: " + reply.summary());
                            return;
                        }
                        if (!sendLine(base64Encode(config_.password)))
                            return;
                        phase_ = Phase::AuthPlain;
                        break;
                    case Phase::MailFrom:
                        if (reply.code != 250 && reply.code != 251) {
                            fail("MAIL FROM rejected: " + reply.summary());
                            return;
                        }
                        if (!sendLine("RCPT TO:<" + rcptTo_ + ">"))
                            return;
                        phase_ = Phase::RcptTo;
                        break;
                    case Phase::RcptTo:
                        if (reply.code != 250 && reply.code != 251) {
                            fail("RCPT TO rejected: " + reply.summary());
                            return;
                        }
                        if (!sendLine("DATA"))
                            return;
                        phase_ = Phase::Data;
                        break;
                    case Phase::Data:
                        if (reply.code != 354) {
                            fail("DATA rejected: " + reply.summary());
                            return;
                        }
                        if (!writeBytes(payload_, true))
                            return;
                        phase_ = Phase::Body;
                        break;
                    case Phase::Body:
                        if (reply.code != 250) {
                            fail("Message rejected: " + reply.summary());
                            return;
                        }
                        succeed();
                        break;
                }
            }

            void startAuthOrMail() {
                if (!config_.user.empty()) {
                    const bool plain = caps_.hasAuthMechanism("PLAIN");
                    const bool login = caps_.hasAuthMechanism("LOGIN");
                    if (!plain && !login) {
                        fail("SMTP server does not advertise AUTH");
                        return;
                    }
                    const auto token = base64Encode(std::string("\0", 1) + config_.user +
                                                    std::string("\0", 1) + config_.password);
                    if (plain && token.size() + 11 < 400) {
                        if (!sendLine("AUTH PLAIN " + token))
                            return;
                        phase_ = Phase::AuthPlain;
                        return;
                    }
                    if (!sendLine("AUTH LOGIN"))
                        return;
                    phase_ = Phase::AuthLoginChallenge;
                    return;
                }
                sendMailFrom();
            }

            void sendMailFrom() {
                if (!sendLine("MAIL FROM:<" + mailFrom_ + ">"))
                    return;
                phase_ = Phase::MailFrom;
            }

            void succeed() {
                if (completed_.load())
                    return;
                writeBytes("QUIT\r\n", false);
                complete(DeliveryResult{true, {}});
            }

            bool sendLine(const std::string &line) {
                std::string data = line;
                if (!data.ends_with("\r\n"))
                    data += "\r\n";
                return writeBytes(data, true);
            }

            bool writeBytes(const std::string &data, bool failOnError) {
                if (completed_.load() || !conn_) {
                    if (failOnError && !completed_.load())
                        fail("SMTP connection is closed");
                    return false;
                }
                if (!handshakeDone_) {
                    conn_->send(data);
                    return true;
                }

                std::size_t offset = 0;
                int stalls = 0;
                while (offset < data.size()) {
                    const int chunk = static_cast<int>(std::min<std::size_t>(data.size() - offset, 16 * 1024));
                    const int n = wolfSSL_write(ssl_, data.data() + offset, chunk);
                    if (n > 0) {
                        offset += static_cast<std::size_t>(n);
                        stalls = 0;
                        continue;
                    }
                    const int err = wolfSSL_get_error(ssl_, n);
                    if (err == WOLFSSL_ERROR_WANT_READ || err == WOLFSSL_ERROR_WANT_WRITE) {
                        if (++stalls > 16) {
                            if (failOnError)
                                fail("TLS write stalled");
                            return false;
                        }
                        continue;
                    }
                    if (failOnError)
                        fail("TLS write failed: " + tlsErrorString(err));
                    return false;
                }
                return true;
            }

            void complete(DeliveryResult result) {
                if (completed_.exchange(true))
                    return;
                result_ = std::move(result);
                auto self = shared_from_this();
                loop_->queueInLoop([self] {
                    if (self->ssl_ != nullptr) {
                        wolfSSL_free(self->ssl_);
                        self->ssl_ = nullptr;
                    }
                    if (self->client_) {
                        self->client_->stop();
                        self->client_->disconnect();
                        self->client_.reset();
                    }
                    self->conn_.reset();
                    try {
                        self->prom_->set_value(self->result_);
                    } catch (const std::future_error &) {
                    }
                });
            }

            static int ioRecv(WOLFSSL *, char *buf, int sz, void *ctx) {
                return static_cast<Session *>(ctx)->consumeCipher(buf, sz);
            }

            static int ioSend(WOLFSSL *, char *buf, int sz, void *ctx) {
                return static_cast<Session *>(ctx)->emitCipher(buf, sz);
            }

            trantor::EventLoop *loop_ = nullptr;
            trantor::InetAddress address_;
            MbSmtpConfig config_;
            std::string mailFrom_;
            std::string rcptTo_;
            std::string payload_;
            WOLFSSL_CTX *ctx_ = nullptr;
            std::shared_ptr<std::promise<DeliveryResult>> prom_;
            std::string ehlo_;
            bool implicit_ = false;
            bool ipLiteral_ = false;

            std::shared_ptr<trantor::TcpClient> client_;
            trantor::TcpConnectionPtr conn_;
            trantor::TimerId timer_ = trantor::InvalidTimerId;
            WOLFSSL *ssl_ = nullptr;

            Phase phase_ = Phase::Greeting;
            SmtpReply caps_;
            std::string pending_;
            std::string cipher_;
            bool tlsActive_ = false;
            bool handshaking_ = false;
            bool handshakeDone_ = false;
            std::atomic<bool> completed_{false};
            DeliveryResult result_;
        };
    } // namespace

    void deliver(const MbSmtpConfig &config,
                 const std::string &mailFrom,
                 const std::string &rcptTo,
                 std::string payload) {
        WOLFSSL_CTX *ctx = nullptr;
        if (config.use_tls) {
            const auto &env = tlsEnv();
            if (env.ctx == nullptr) {
                throw MbException(502, env.error.empty() ? "SMTP TLS is unavailable" : env.error);
            }
            ctx = env.ctx;
        }

        std::string dnsError;
        auto address = resolveHost(config.host, config.port, dnsError);
        if (!address)
            throw MbException(502, dnsError);

        trantor::EventLoopThread loopThread("mb-smtp");
        loopThread.run();

        auto prom = std::make_shared<std::promise<DeliveryResult>>();
        auto fut = prom->get_future();
        auto session = std::make_shared<Session>(loopThread.getLoop(), std::move(*address), config,
                                                 mailFrom, rcptTo, std::move(payload), ctx, prom);
        loopThread.getLoop()->runInLoop([session] { session->start(); });

        DeliveryResult result;
        if (fut.wait_for(std::chrono::seconds(25)) == std::future_status::ready) {
            result = fut.get();
        } else {
            session->fail("SMTP connection timed out");
            if (fut.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
                result = fut.get();
            else
                result = DeliveryResult{false, "SMTP connection timed out"};
        }

        session.reset();
        if (!result.ok) {
            throw MbException(502, result.error.empty() ? "SMTP delivery failed" : result.error);
        }
    }
} // namespace mb::smtp_detail
