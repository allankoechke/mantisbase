#include "../../include/mantisbase/core/models/entity_routes.h"
#include "../../include/mantisbase/core/models/entity.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/middlewares.h"
#include "../../include/mantisbase/mantisbase.h"

namespace mb {
    namespace {
        void handleGetOne(const MbRequest &req, const MbResponse &res, const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                const auto entity_id = trim(req.getPathParamValue("id"));
                if (entity_id.empty())
                    throw MantisException(400, "Entity `id` is required!");

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
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("Entity", "Handler Error", fmt::format("Entity handler error: {}", e.what()));
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
                                ? safe_stoi(req.getQueryParamValue("limit"), 50)
                                : 50;

                std::string after = req.hasQueryParam("after")
                                        ? req.getQueryParamValue("after")
                                        : "";

                std::string filter = req.hasQueryParam("filter")
                                         ? req.getQueryParamValue("filter")
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
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("Entity", "Handler Error", fmt::format("Entity handler error: {}", e.what()));
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", "An internal error occurred."},
                                 {"status", 500}
                             });
            }
        }

        void handlePost(const MbRequest &req, const MbResponse &res, MbContentReader &reader,
                        const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                reader.parseFormDataToEntity(entity);

                if (const auto &val_err = Validators::validateRequestBody(entity, reader.jsonBody());
                    val_err.has_value()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", val_err.value()},
                                     {"status", 400}
                                 });
                    return;
                }

                reader.writeFiles(entity_name);
                auto record = entity.create(reader.jsonBody());

                if (entity.type() == "auth" && record.contains("password")) {
                    record.erase("password");
                }

                res.sendJSON(201, {
                                 {"data", record},
                                 {"error", ""},
                                 {"status", 201}
                             });
            } catch (const MantisException &e) {
                reader.undoWrittenFiles(entity_name);
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                reader.undoWrittenFiles(entity_name);
                res.sendJSON(500, {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", 500}
                             });
            }
        }

        void handlePatch(MbRequest &req, MbResponse &res, MbContentReader &reader,
                         const std::string &entity_name) {
            try {
                const auto entity = req.mbApp().entity(entity_name);

                const auto entity_id = trim(req.getPathParamValue("id"));
                if (entity_id.empty())
                    throw MantisException(400, "Entity `id` is required!");

                reader.parseFormDataToEntity(entity);

                if (const auto &val_err = Validators::validateUpdateRequestBody(entity, reader.jsonBody());
                    val_err.has_value()) {
                    res.sendJSON(400, {
                                     {"data", json::object()},
                                     {"error", val_err.value()},
                                     {"status", 400}
                                 });
                    return;
                }

                reader.writeFiles(entity_name);
                auto record = entity.update(entity_id, reader.jsonBody());

                if (entity.type() == "auth" && record.contains("password")) {
                    record.erase("password");
                }

                res.sendJSON(200, {
                                 {"data", record},
                                 {"error", ""},
                                 {"status", 200}
                             });
            } catch (const MantisException &e) {
                reader.undoWrittenFiles(entity_name);
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                reader.undoWrittenFiles(entity_name);
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

                const auto entity_id = trim(req.getPathParamValue("id"));
                if (entity_id.empty())
                    throw MantisException(400, "Entity `id` is required!");

                entity.remove(entity_id);
                res.sendEmpty();
            } catch (const MantisException &e) {
                res.sendJSON(e.code(), {
                                 {"data", json::object()},
                                 {"error", e.what()},
                                 {"status", e.code()}
                             });
            } catch (const std::exception &e) {
                req.mbApp().logger().critical("Entity", "Handler Error", fmt::format("Entity handler error: {}", e.what()));
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
            handleGetOne(req, res, trim(req.getPathParamValue("entity_name")));
        };
    }

    MbHandlerFn entityGetManyHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            handleGetMany(req, res, trim(req.getPathParamValue("entity_name")));
        };
    }

    MbHandlerWithContentReaderFn entityPostHandler() {
        return [](const MbRequest &req, const MbResponse &res, MbContentReader &reader) {
            handlePost(req, res, reader, trim(req.getPathParamValue("entity_name")));
        };
    }

    MbHandlerWithContentReaderFn entityPatchHandler() {
        return [](MbRequest &req, MbResponse &res, MbContentReader &reader) {
            handlePatch(req, res, reader, trim(req.getPathParamValue("entity_name")));
        };
    }

    MbHandlerFn entityDeleteHandler() {
        return [](const MbRequest &req, const MbResponse &res) {
            handleDelete(req, res, trim(req.getPathParamValue("entity_name")));
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
                    [admin_entity](const MbRequest &req, const MbResponse &res, MbContentReader &reader) {
                        handlePost(req, res, reader, admin_entity);
                    },
                    {
                        requireAdminAuth(),
                        settingsFeatureGate("disableAdminRegistration"),
                        envGateMiddleware("MB_DISABLE_ADMIN_MUTATIONS", true)
                    });
        router.Patch("/api/v1/sys/admins/:id",
                     [admin_entity](MbRequest &req, MbResponse &res, MbContentReader &reader) {
                         handlePatch(req, res, reader, admin_entity);
                     },
                     {requireAdminAuth(), envGateMiddleware("MB_DISABLE_ADMIN_MUTATIONS", true)});
        router.Delete("/api/v1/sys/admins/:id",
                      [admin_entity](const MbRequest &req, const MbResponse &res) {
                          handleDelete(req, res, admin_entity);
                      },
                      {requireAdminAuth(), envGateMiddleware("MB_DISABLE_ADMIN_MUTATIONS", true)});
    }
}
