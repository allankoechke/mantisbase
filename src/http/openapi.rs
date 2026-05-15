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
                    "bearerFormat": "JWT"
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
                    "security": [{ "basicAuth": [] }],
                    "summary": "List entity schemas (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "post": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "Create schema (admin)",
                    "responses": { "201": { "description": "Created" } }
                }
            },
            "/api/v1/sys/schemas/{name}": {
                "get": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "Get schema (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "patch": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "Patch schema (admin); reconciles physical table and writes generated SQL",
                    "responses": { "200": { "description": "OK" } }
                },
                "delete": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "Delete schema (admin)",
                    "responses": { "204": { "description": "No Content" } }
                }
            },
            "/api/v1/sys/configs": {
                "get": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "List application config (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "patch": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "Patch config key (admin)",
                    "responses": { "200": { "description": "OK" } }
                }
            },
            "/api/v1/sys/logs": {
                "get": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "System logs (admin)",
                    "parameters": [
                        { "name": "limit", "in": "query", "required": false, "schema": { "type": "integer", "maximum": 500 } }
                    ],
                    "responses": { "200": { "description": "OK" } }
                }
            },
            "/api/v1/admins": {
                "get": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "List admin accounts (id, email, active, password_reset_required) (admin)",
                    "responses": { "200": { "description": "OK" } }
                },
                "post": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "Create admin account (admin)",
                    "responses": { "201": { "description": "Created" } }
                }
            },
            "/api/v1/admins/{id}": {
                "delete": {
                    "security": [{ "basicAuth": [] }],
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
