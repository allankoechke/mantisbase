#include "../../include/mantisbase/core/models/entity_routes.h"
#include "../../include/mantisbase/core/models/entity.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/middlewares.h"
#include "../../include/mantisbase/mantisbase.h"

namespace mb {
    namespace {
        json authWriteBody(MbRequest &req, const MbEntity &entity, const json &existing = json{}) {
            json body = req.jsonBody();
            if (entity.type() != "auth" || entity.isSystem() || !entity.field("email_verified").has_value())
                return body;

            const bool admin = req.isAdminAuth();
            if (!admin)
                body.erase("email_verified");
            else if (body.contains("email_verified") && !body["email_verified"].is_boolean())
                throw MbException(400, "Expected `email_verified` to be a boolean.");

            if (!existing.is_object() || existing.empty())
                return body;
            if (!body.contains("email") || !body["email"].is_string())
                return body;

            const auto previous = existing.contains("email") && existing["email"].is_string()
                                      ? trim(existing["email"].get<std::string>())
                                      : std::string{};
            auto next = trim(body["email"].get<std::string>());
            auto previous_key = previous;
            auto next_key = next;
            toLowerCase(previous_key);
            toLowerCase(next_key);
            if (!next_key.empty() && previous_key != next_key && !(admin && body.contains("email_verified")))
                body["email_verified"] = false;
            return body;
        }

        void maybeSendVerification(const MbRequest &req, const MbEntity &entity, const json &record) {
            if (entity.type() != "auth" || entity.isSystem() || !record.contains("id") || !record["id"].is_string())
                return;
            if (record.contains("email_verified") && record["email_verified"].is_boolean() &&
                record["email_verified"].get<bool>())
                return;
            req.mbApp().auth().notifyEmailVerification(entity.name(), record["id"].get<std::string>());
        }

        void handleGetOne(const MbRequest &req, const MbResponse &res, const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                const auto entity_id = trim(req.pathParamValue("id"));
                if (entity_id.empty())
                    throw MbException(400, "Entity `id` is required!");

                if (const auto record = entity.read(entity_id); record.has_value()) {
                    res.sendJSON(200, {
                                     {"data", record},
                                     {"error", ""},
                                     {"status", 200}
                                 });
                } else {
                    res.sendJSON(404, {
                                     {"data", json::object()},
                                     {"error", "Resource not found!"},
                                     {"status", 404}
                                 });
                }
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbEntity", "Handler Error", fmt::format("MbEntity handler error: {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        }

        void handleGetMany(const MbRequest &req, const MbResponse &res, const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                int limit = req.hasQueryParam("limit")
                                ? safe_stoi(req.queryParamValue("limit"), 50)
                                : 50;

                std::string after = req.hasQueryParam("after")
                                        ? req.queryParamValue("after")
                                        : "";

                std::string filter = req.hasQueryParam("filter")
                                         ? req.queryParamValue("filter")
                                         : "";

                nlohmann::json opts;
                opts["pagination"] = {
                    {"limit", limit},
                    {"after", after}
                };
                opts["filter"] = filter;

                const auto page = entity.listPage(opts);

                std::string cursor;
                if (!page.items.empty()) {
                    const auto &last = page.items.back();
                    if (last.contains("id") && last["id"].is_string())
                        cursor = last["id"].get<std::string>();
                }

                res.sendJSON(200, {
                                 {
                                     "data", {
                                         {"items_count", page.items.size()},
                                         {"limit", limit},
                                         {"has_more", page.has_more},
                                         {"cursor", cursor},
                                         {"items", page.items}
                                     }
                                 },
                                 {"error", ""},
                                 {"status", 200}
                             });
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbEntity", "Handler Error", fmt::format("MbEntity handler error: {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        }

        void handlePost(MbRequest &req, const MbResponse &res,
                        const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                req.parseFormDataToEntity(entity);

                if (const auto &val_err = MbValidators::validateRequestBody(entity, req.jsonBody());
                    val_err.has_value()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", val_err.value()},
                                     {"status", 400}
                                 });
                    return;
                }

                req.writeFiles(entity_name);
                auto record = entity.create(authWriteBody(req, entity));

                if (entity.type() == "auth" && record.contains("password")) {
                    record.erase("password");
                }
                maybeSendVerification(req, entity, record);

                res.sendJSON(201, {
                                 {"data", record},
                                 {"error", ""},
                                 {"status", 201}
                             });
            } catch (const MbException &e) {
                req.undoWrittenFiles(entity_name);
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.undoWrittenFiles(entity_name);
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             });
            }
        }

        void handlePatch(MbRequest &req, const MbResponse &res,
                         const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                const auto entity_id = trim(req.pathParamValue("id"));
                if (entity_id.empty())
                    throw MbException(400, "Entity `id` is required!");

                req.parseFormDataToEntity(entity);

                if (const auto &val_err = MbValidators::validateUpdateRequestBody(entity, req.jsonBody());
                    val_err.has_value()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", val_err.value()},
                                     {"status", 400}
                                 });
                    return;
                }

                json existing = json::object();
                if (entity.type() == "auth" && !entity.isSystem()) {
                    if (const auto current = entity.read(entity_id); current.has_value())
                        existing = *current;
                }

                req.writeFiles(entity_name);
                auto record = entity.update(entity_id, authWriteBody(req, entity, existing));

                if (entity.type() == "auth" && record.contains("password")) {
                    record.erase("password");
                }
                if (existing.contains("email") && record.contains("email") &&
                    existing["email"] != record["email"])
                    maybeSendVerification(req, entity, record);

                res.sendJSON(200, {
                                 {"data", record},
                                 {"error", ""},
                                 {"status", 200}
                             });
            } catch (const MbException &e) {
                req.undoWrittenFiles(entity_name);
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.undoWrittenFiles(entity_name);
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             });
            }
        }

        void handleDelete(const MbRequest &req, const MbResponse &res, const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                const auto entity_id = trim(req.pathParamValue("id"));
                if (entity_id.empty())
                    throw MbException(400, "Entity `id` is required!");

                entity.remove(entity_id);
                res.sendEmpty();
            } catch (const MbException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("MbEntity", "Handler Error", fmt::format("MbEntity handler error: {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        }
    }

    MbHandlerFn entityGetOneHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            handleGetOne(req, res, trim(req.pathParamValue("entity_name")));
        };
    }

    MbHandlerFn entityGetManyHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            handleGetMany(req, res, trim(req.pathParamValue("entity_name")));
        };
    }

    MbHandlerFn entityPostHandler() {
        return [](MbRequest &req, const MbResponse &res) {
            handlePost(req, res, trim(req.pathParamValue("entity_name")));
        };
    }

    MbHandlerFn entityPatchHandler() {
        return [](MbRequest &req, const MbResponse &res) {
            handlePatch(req, res, trim(req.pathParamValue("entity_name")));
        };
    }

    MbHandlerFn entityDeleteHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            handleDelete(req, res, trim(req.pathParamValue("entity_name")));
        };
    }

    void registerAdminEntityRoutes(const MantisBase &app) {
        auto &router = app.router();
        const std::string admin_entity = "mb_admins";

        router.Get("/api/v1/sys/admins",
                   [admin_entity](const MbRequest &req, const MbResponse &res) {
                       handleGetMany(req, res, admin_entity);
                   },
                   {requireAdminAuth()});
        router.Get("/api/v1/sys/admins/:id",
                   [admin_entity](const MbRequest &req, const MbResponse &res) {
                       handleGetOne(req, res, admin_entity);
                   },
                   {requireAdminAuth()});
        router.Post("/api/v1/sys/admins",
                    [admin_entity](MbRequest &req, const MbResponse &res) {
                        handlePost(req, res, admin_entity);
                    },
                    {
                        requireAdminAuth(),
                        settingsFeatureGate("disableAdminRegistration"),
                        envGateMiddleware("MB_DISABLE_ADMIN_MUTATIONS", true)
                    });
        router.Patch("/api/v1/sys/admins/:id",
                     [admin_entity](MbRequest &req, const MbResponse &res) {
                         handlePatch(req, res, admin_entity);
                     },
                     {requireAdminAuth(), envGateMiddleware("MB_DISABLE_ADMIN_MUTATIONS", true)});
        router.Delete("/api/v1/sys/admins/:id",
                      [admin_entity](const MbRequest &req, const MbResponse &res) {
                          handleDelete(req, res, admin_entity);
                      },
                      {requireAdminAuth(), envGateMiddleware("MB_DISABLE_ADMIN_MUTATIONS", true)});
    }
}
