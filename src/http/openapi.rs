//! Minimal OpenAPI 3.0 document (expanded later from catalog).

use serde_json::{json, Value};

pub fn build_openapi_value(entities: &[Value]) -> Value {
    json!({
        "openapi": "3.0.3",
        "info": {
            "title": "MantisBase API",
            "version": env!("CARGO_PKG_VERSION"),
        },
        "components": {
            "securitySchemes": {
                "basicAuth": {
                    "type": "http",
                    "scheme": "basic"
                },
                "bearerAuth": {
                    "type": "http",
                    "scheme": "bearer",
                    "bearerFormat": "JWT",
                    "description": "Application user JWT from /api/v1/auth/login, or admin JWT from /api/v1/admins/auth/login (aud=mantisbase_admin)"
                }
            }
        },
        "paths": {
            "/api/v1/health": {
                "get": {
                    "summary": "Health check",
                    "responses": { "200": { "description": "OK" } }
                }
            },
            "/api/v1/sys/schemas": {
                "get": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "List entity schemas (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "post": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Create schema (admin)",
                    "responses": { "201": { "description": "Created" } }
                }
            },
            "/api/v1/sys/schemas/{name}": {
                "get": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Get schema (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "patch": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Patch schema (admin); reconciles physical table and writes generated SQL",
                    "responses": { "200": { "description": "OK" } }
                },
                "delete": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Delete schema (admin)",
                    "responses": { "204": { "description": "No Content" } }
                }
            },
            "/api/v1/sys/configs": {
                "get": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "List application config (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "patch": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Patch config key (admin)",
                    "responses": { "200": { "description": "OK" } }
                }
            },
            "/api/v1/sys/logs": {
                "get": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "System logs (admin)",
                    "parameters": [
                        { "name": "limit", "in": "query", "required": false, "schema": { "type": "integer", "maximum": 500 } }
                    ],
                    "responses": { "200": { "description": "OK" } }
                }
            },
            "/api/v1/setup/status": {
                "get": {
                    "summary": "First-admin setup status (public when no admins exist)",
                    "parameters": [
                        { "name": "token", "in": "query", "required": false, "schema": { "type": "string" } }
                    ],
                    "responses": { "200": { "description": "OK" } }
                }
            },
            "/api/v1/setup/first-admin": {
                "post": {
                    "summary": "Create first admin using setup token (Bearer or body.token)",
                    "responses": { "201": { "description": "Created" }, "401": { "description": "Invalid token" }, "403": { "description": "Setup not available" } }
                }
            },
            "/api/v1/admins/auth/login": {
                "post": {
                    "summary": "Admin login (email + password); returns JWT and admin profile (no prior auth)",
                    "requestBody": {
                        "required": true,
                        "content": {
                            "application/json": {
                                "schema": {
                                    "type": "object",
                                    "required": ["email", "password"],
                                    "properties": {
                                        "email": { "type": "string" },
                                        "password": { "type": "string" }
                                    }
                                }
                            }
                        }
                    },
                    "responses": {
                        "200": { "description": "OK; body includes token (Bearer) and admin object" },
                        "400": { "description": "Bad request" },
                        "401": { "description": "Invalid credentials" }
                    }
                }
            },
            "/api/v1/admins": {
                "get": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "List admin accounts (id, email, active, password_reset_required) (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "post": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Create admin account (admin)",
                    "responses": { "201": { "description": "Created" } }
                }
            },
            "/api/v1/admins/{id}": {
                "get": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Get admin by id or email (admin)",
                    "parameters": [{ "name": "id", "in": "path", "required": true, "schema": { "type": "string" } }],
                    "responses": { "200": { "description": "OK" }, "404": { "description": "Not found" } }
                },
                "delete": {
                    "security": [{ "basicAuth": [] }, { "bearerAuth": [] }],
                    "summary": "Delete admin by id or email (admin)",
                    "parameters": [{ "name": "id", "in": "path", "required": true, "schema": { "type": "string" } }],
                    "responses": { "204": { "description": "No Content" }, "404": { "description": "Not found" } }
                }
            },
            "/api/v1/entities/{entity}": {
                "get": {
                    "summary": "List rows",
                    "parameters": [{ "name": "entity", "in": "path", "required": true, "schema": { "type": "string" } }],
                    "responses": { "200": { "description": "OK" } }
                },
                "post": {
                    "summary": "Create row",
                    "parameters": [{ "name": "entity", "in": "path", "required": true, "schema": { "type": "string" } }],
                    "responses": { "201": { "description": "Created" } }
                }
            }
        },
        "x-catalog-entities": entities,
    })
}
