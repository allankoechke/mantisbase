//
// Created by codeart on 17/11/2025.
//

#include "../../include/mantisbase/core/router.h"

namespace mantis {
    std::function<HandlerResponse(const httplib::Request &, httplib::Response &)> Router::preRoutingHandler() {
        return [](const httplib::Request& req, httplib::Response& _) -> HandlerResponse {
            auto &mutable_req = const_cast<httplib::Request &>(req);
            mutable_req.start_time_ = std::chrono::steady_clock::now(); // Set the start time
            return HandlerResponse::Unhandled;
        };
    }

    std::function<void(const httplib::Request &, httplib::Response &)> Router::postRoutingHandler() {
        return [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            res.set_header("Access-Control-Max-Age", "86400");
        };
    }

    std::function<void(const httplib::Request &, const httplib::Response &)> Router::routingLogger() {
        return [this](const httplib::Request& req, const httplib::Response& res) {
            // Calculate execution time (if start_time was set)
            const auto end_time = std::chrono::steady_clock::now();
            const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - req.start_time_).count();

            if (res.status < 400)
            {
                logger::info("{} {:<7} {}  - Status: {}  - Time: {}ms",
                          req.version, req.method, req.path, res.status, duration_ms);
            }
            else
            {
                // Decompress if content is compressed
                if (res.body.empty())
                {
                    logger::info("{} {:<7} {}  - Status: {}  - Time: {}ms",
                              req.version, req.method, req.path, res.status, duration_ms);
                }
                else
                {
                    // Get the compression encoding
                    const std::string encoding = res.get_header_value("Content-Encoding");

                    auto body = encoding.empty() ? res.body : decompressResponseBody(res.body, encoding);
                    logger::info("{} {:<7} {}  - Status: {}  - Time: {}ms\n\t└──Body: {}",
                              req.version, req.method, req.path, res.status, duration_ms, body);
                }
            }
        };
    }

    std::function<void(const httplib::Request &, httplib::Response &)> Router::routingErrorHandler() {
        return [](const httplib::Request&, httplib::Response& res) {
            if (res.body.empty()) {
                json response;
                response["status"] = res.status;
                response["data"] = json::object();

                if (res.status == 404)
                    response["error"] = "Resource not found!";
                else if (res.status >= 500)
                    response["error"] = "Internal server error, try again later!";
                else
                    response["error"] = "Something went wrong here!";

                res.set_content(response.dump(), "application/json");
            }
        };
    }

    std::function<void(const httplib::Request &, httplib::Response &)> Router::optionsHandler() {
        return [](const httplib::Request&, httplib::Response& res) {
            // Headers are already set by post_routing_handler
            res.status = 200;
        };
    }

    std::string Router::decompressResponseBody(const std::string& body, const std::string& encoding)
    {
        std::string decompressed_content;

        if (encoding == "gzip" || encoding == "deflate")
        {
#ifdef CPPHTTPLIB_ZLIB_SUPPORT
            httplib::detail::gzip_decompressor decompressor;
            if (decompressor.is_valid())
            {
                decompressor.decompress(
                    body.data(), body.size(),
                    [&](const char* data, const size_t len)
                    {
                        decompressed_content.append(data, len);
                        return true;
                    }
                );
            }
#endif
        }
        else if (encoding.find("br") != std::string::npos)
        {
#ifdef CPPHTTPLIB_BROTLI_SUPPORT
            httplib::detail::brotli_decompressor decompressor;
            if (decompressor.is_valid())
            {
                decompressor.decompress(
                    body.data(), body.size(),
                    [&](const char* data, const size_t len)
                    {
                        decompressed_content.append(data, len);
                        return true;
                    }
                );
            }
#endif
        }
        else if (encoding == "zstd")
        {
#ifdef CPPHTTPLIB_ZSTD_SUPPORT
            httplib::detail::zstd_decompressor decompressor;
            if (decompressor.is_valid())
            {
                decompressor.decompress(
                    body.data(), body.size(),
                    [&](const char* data, const size_t len)
                    {
                        decompressed_content.append(data, len);
                        return true;
                    }
                );
            }
#endif
        }

        return decompressed_content;
    }
}