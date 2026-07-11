#ifndef MANTISBASE_TEST_HTTP_CLIENT_H
#define MANTISBASE_TEST_HTTP_CLIENT_H

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <utility>
#include <future>
#include <drogon/HttpClient.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <trantor/net/EventLoopThread.h>

namespace TestHttp {

using Headers = std::vector<std::pair<std::string, std::string>>;

struct Response {
    int status = 0;
    std::string body;
    std::map<std::string, std::string> headers;

    explicit operator bool() const { return status > 0; }
};

class Client {
public:
    Client(const std::string& host, int port)
        : host_(host), port_(port) {
        loopThread_.run();
        auto url = "http://" + host + ":" + std::to_string(port);
        client_ = drogon::HttpClient::newHttpClient(url, loopThread_.getLoop());
        client_->setUserAgent("MantisBase-Test/1.0");
    }

    ~Client() = default;

    void set_connection_timeout(int sec, int usec) {
        // Drogon client handles timeouts differently
    }

    void set_read_timeout(int sec, int usec) {
        // Drogon client handles timeouts differently
    }

    std::unique_ptr<Response> Get(const std::string& path, const Headers& headers = {}) {
        return sendRequest(drogon::Get, path, headers);
    }

    std::unique_ptr<Response> Post(const std::string& path,
                                    const std::string& body,
                                    const std::string& content_type) {
        return sendRequest(drogon::Post, path, {}, body, content_type);
    }

    std::unique_ptr<Response> Post(const std::string& path,
                                    const Headers& headers,
                                    const std::string& body,
                                    const std::string& content_type) {
        return sendRequest(drogon::Post, path, headers, body, content_type);
    }

    std::unique_ptr<Response> Patch(const std::string& path,
                                     const Headers& headers,
                                     const std::string& body,
                                     const std::string& content_type) {
        return sendRequest(drogon::Patch, path, headers, body, content_type);
    }

    std::unique_ptr<Response> Delete(const std::string& path,
                                      const Headers& headers = {}) {
        return sendRequest(drogon::Delete, path, headers);
    }

private:
    std::unique_ptr<Response> sendRequest(drogon::HttpMethod method,
                                           const std::string& path,
                                           const Headers& headers,
                                           const std::string& body = "",
                                           const std::string& content_type = "") {
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(method);
        req->setPath(path);

        for (const auto& [key, value] : headers) {
            req->addHeader(key, value);
        }

        if (!body.empty()) {
            req->setBody(body);
            if (!content_type.empty()) {
                req->setContentTypeString(content_type);
            }
        }

        auto promise = std::make_shared<std::promise<std::unique_ptr<Response>>>();
        auto future = promise->get_future();

        client_->sendRequest(req, [promise](drogon::ReqResult result,
                                             const drogon::HttpResponsePtr& resp) {
            auto r = std::make_unique<Response>();
            if (result == drogon::ReqResult::Ok && resp) {
                r->status = static_cast<int>(resp->statusCode());
                r->body = std::string(resp->body());
            }
            promise->set_value(std::move(r));
        }, 10.0);  // 10 second timeout

        return future.get();
    }

    std::string host_;
    int port_;
    trantor::EventLoopThread loopThread_;
    drogon::HttpClientPtr client_;
};

} // namespace TestHttp

#endif // MANTISBASE_TEST_HTTP_CLIENT_H
