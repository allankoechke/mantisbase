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
            "/api/v1/schemas": {
                "get": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "List entity schemas",
                    "responses": { "200": { "description": "OK" } }
                },
                "post": {
                    "security": [{ "basicAuth": [] }],
                    "summary": "Create schema",
                    "responses": { "201": { "description": "Created" } }
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
