#include "../../include/mantisbase/core/models/entity_routes.h"
#include "../../include/mantisbase/core/models/entity.h"
#include "../../include/mantisbase/core/auth.h"
#include "../../include/mantisbase/core/middlewares.h"
#include "../../include/mantisbase/mantisbase.h"

namespace mb {
    namespace {
        void handleGetOne(const MantisRequest &req, const MantisResponse &res, const std::string &entity_name) {
            try {
                const auto entity = req.app().entity(entity_name);

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
                res.sendJSON(500, {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", 500}
                });
            }
        }

        void handleGetMany(const MantisRequest &req, const MantisResponse &res, const std::string &entity_name) {
            try {
                const auto entity = req.app().entity(entity_name);

                int limit = req.hasQueryParam("limit")
                                ? safe_stoi(req.getQueryParamValue("limit"), 50)
                                : 50;

                std::string after = req.hasQueryParam("after")
                                        ? req.getQueryParamValue("after")
                                        : "";

                std::string sort = req.hasQueryParam("sort")
                                       ? req.getQueryParamValue("sort")
                                       : "";

                std::string filter = req.hasQueryParam("filter")
                                         ? req.getQueryParamValue("filter")
                                         : "";

                nlohmann::json opts;
                opts["pagination"] = {
                    {"limit", limit},
                    {"after", after},
                    {"sort", sort}
                };
                opts["filter"] = filter;

                const auto records = entity.list(opts);

                std::string cursor;
                if (!records.empty()) {
                    const auto &last = records.back();
                    if (last.contains("id") && last["id"].is_string())
                        cursor = last["id"].get<std::string>();
                }

                res.sendJSON(200, {
                    {
                        "data", {
                            {"items_count", records.size()},
                            {"limit", limit},
                            {"cursor", cursor},
                            {"items", records}
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
                res.sendJSON(500, {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", 500}
                });
            }
        }

        void handlePost(const MantisRequest &req, const MantisResponse &res, MantisContentReader &reader,
                        const std::string &entity_name) {
            try {
                const auto entity = req.app().entity(entity_name);

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

        void handlePatch(MantisRequest &req, MantisResponse &res, MantisContentReader &reader,
                         const std::string &entity_name) {
            try {
                const auto entity = req.app().entity(entity_name);

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

        void handleDelete(const MantisRequest &req, const MantisResponse &res, const std::string &entity_name) {
            try {
                const auto entity = req.app().entity(entity_name);

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
                res.sendJSON(500, {
                    {"data", json::object()},
                    {"error", e.what()},
                    {"status", 500}
                });
            }
        }
    }

    HandlerFn entityGetOneHandler() {
        return [](const MantisRequest &req, const MantisResponse &res) {
            handleGetOne(req, res, trim(req.getPathParamValue("entity_name")));
        };
    }

    HandlerFn entityGetManyHandler() {
        return [](const MantisRequest &req, const MantisResponse &res) {
            handleGetMany(req, res, trim(req.getPathParamValue("entity_name")));
        };
    }

    HandlerWithContentReaderFn entityPostHandler() {
        return [](const MantisRequest &req, const MantisResponse &res, MantisContentReader &reader) {
            handlePost(req, res, reader, trim(req.getPathParamValue("entity_name")));
        };
    }

    HandlerWithContentReaderFn entityPatchHandler() {
        return [](MantisRequest &req, MantisResponse &res, MantisContentReader &reader) {
            handlePatch(req, res, reader, trim(req.getPathParamValue("entity_name")));
        };
    }

    HandlerFn entityDeleteHandler() {
        return [](const MantisRequest &req, const MantisResponse &res) {
            handleDelete(req, res, trim(req.getPathParamValue("entity_name")));
        };
    }

    void registerAdminEntityRoutes() {
        auto &router = MantisBase::instance().router();
        const std::string admin_entity = "mb_admins";

        router.Get("/api/v1/sys/admins",
                   [admin_entity](const MantisRequest &req, const MantisResponse &res) {
                       handleGetMany(req, res, admin_entity);
                   },
                   {requireAdminAuth()});
        router.Get("/api/v1/sys/admins/:id",
                   [admin_entity](const MantisRequest &req, const MantisResponse &res) {
                       handleGetOne(req, res, admin_entity);
                   },
                   {requireAdminAuth()});
        router.Post("/api/v1/sys/admins",
                    [admin_entity](const MantisRequest &req, const MantisResponse &res, MantisContentReader &reader) {
                        handlePost(req, res, reader, admin_entity);
                    },
                    {requireAdminAuth()});
        router.Patch("/api/v1/sys/admins/:id",
                     [admin_entity](MantisRequest &req, MantisResponse &res, MantisContentReader &reader) {
                         handlePatch(req, res, reader, admin_entity);
                     },
                     {requireAdminAuth()});
        router.Delete("/api/v1/sys/admins/:id",
                      [admin_entity](const MantisRequest &req, const MantisResponse &res) {
                          handleDelete(req, res, admin_entity);
                      },
                      {requireAdminAuth()});
    }
}
