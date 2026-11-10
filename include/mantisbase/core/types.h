/**
 * @file types.h
 * @brief Core type aliases, handler signatures, and the MbAppMixin DI mixin.
 *
 * Defines the function types used throughout routing (@ref HandlerFn, @ref MiddlewareFn)
 * and the non-owning @ref MantisBase reference mixin inherited by request/response wrappers
 * and service classes.
 */

#ifndef MANTISBASE_TYPES_H
#define MANTISBASE_TYPES_H

#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>
#include <filesystem>

#include "../export.h"

namespace mb {
    class MantisBase;
    class MbRequest;
    class MbResponse;
    class MbEntity;
    class MbEntitySchema;
    class MbEntitySchemaField;

    namespace fs = std::filesystem;

    class MbKVStore;
    class MbDatabase;
    class MbLogger;
    class MbRouter;
    class MbFilesMgr;
    class MbAuth;
    class MbApiKeyMgr;
    class MbOAuthMgr;

    // Email service
    struct MbSmtpConfig;
    struct MbEmailMsg;
    struct MbRateLimitEntry;
    class MbMailer;

    using json = nlohmann::json;

    /**
     * @brief Middleware short-circuit result: stop or continue the chain.
     */
    enum class MbHandlerResponse {
        Handled,   /**< Response already sent; skip remaining middleware/handler. */
        Unhandled  /**< Continue to the next middleware or route handler. */
    };

    /**
     * @brief Standard route handler `(request, response)`.
     *
     * The single handler signature for all methods including PUT/PATCH/POST.
     * Body handling (JSON and multipart/file uploads) is available directly
     * on @ref MbRequest via `jsonBody()`, `formData()`, `filesMetadata()`,
     * `parseFormDataToEntity()`, and `writeFiles()`.
     */
    using MbHandlerFn = std::function<void(MbRequest&, MbResponse&)>;
    using MbMiddlewareFn = std::function<MbHandlerResponse(MbRequest&, MbResponse&)>;
    using MbMiddlewares = std::vector<MbMiddlewareFn>;
    using MbMethod = std::string;
    using MbPath = std::string;

    /**
     * @brief Route lookup key: HTTP method + path pattern.
     */
    using MbRouteKey = std::pair<MbMethod, MbPath>;

    /**
     * @brief Non-owning access to the active @ref MantisBase from DI-aware types.
     *
     * `MbEntity`, `MbRouter`, `MbRequest`, `MbApiKeyMgr`, `MbOAuthMgr`, and
     * other framework types inherit this mixin so handlers and services can call
     * `mbApp()` instead of a removed global singleton.
     *
     * @code
     * router.Get("/api/v1/stats", [](MbRequest& req, MbResponse& res) {
     *     auto count = req.mbApp().entity("posts").countRecords();
     *     res.sendJSON(200, {{"posts", count}});
     * });
     * @endcode
     */
    class MANTISBASE_API MbAppMixin {
        const MantisBase& m_app; ///< Const ref to the parent MantisBase instance //

    public:
        //! Constrictor for instantiating this interface
        explicit MbAppMixin(const MantisBase& app);

        // Move constructor & op
        MbAppMixin(MbAppMixin&&) = default;
        MbAppMixin& operator=(MbAppMixin&&) = delete;

        // Copy constructor & op
        MbAppMixin(const MbAppMixin&) = default;
        MbAppMixin& operator=(const MbAppMixin&) = delete;

        //! Application that owns this service or request context.
        [[nodiscard]] const MantisBase &mbApp() const;

        //! @return Application logger via @ref MantisBase::logger
        [[nodiscard]] const MbLogger& logger() const;
    };

#define REQUEST_HANDLED MbHandlerResponse::Handled;
#define REQUEST_PENDING MbHandlerResponse::Unhandled;
}

#endif //MANTISBASE_TYPES_H
