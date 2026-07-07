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

                auto page = req.hasQueryParam("page")
                                ? safe_stoi(req.getQueryParamValue("page"), 1)
                                : 1;

                auto page_size = req.hasQueryParam("page_size")
                                     ? safe_stoi(req.getQueryParamValue("page_size"), 100)
                                     : 100;

                // Clamp pagination so a single request cannot request an
                // unbounded page. Keep the echoed metadata consistent with the
                // values actually used by the query.
                if (page < 1) page = 1;
                if (page_size < 1) page_size = 1;
                if (page_size > MAX_LIST_PAGE_SIZE) page_size = MAX_LIST_PAGE_SIZE;

                bool skip_total_count = req.hasQueryParam("skip_total_count")
                                            ? strToBool(req.getQueryParamValue("skip_total_count"))
                                            : false;

                std::string filter = req.hasQueryParam("filter")
                                         ? req.getQueryParamValue("filter")
                                         : "";

                nlohmann::json opts;
                opts["pagination"] = {
                    {"page", page},
                    {"page_size", page_size},
                    {"skip_total_count", skip_total_count}
                };
                opts["filter"] = filter;

                int items_count = skip_total_count ? -1 : entity.countRecords();
                const auto records = entity.list(opts);

                res.sendJSON(200, {
                    {
                        "data", {
                            {"page", page},
                            {"items_count", records.size()},
                            {"page_size", page_size},
                            {"total_count", items_count},
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
