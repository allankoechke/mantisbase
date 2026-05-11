//! Axum HTTP server: `/api/v1/*`, admin Basic auth, OpenAPI, static `/mb`.

pub mod openapi;

use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

use argon2::password_hash::PasswordHasher;
use axum::extract::{Path, Query, State};
use axum::http::{header, HeaderMap, StatusCode};
use axum::response::IntoResponse;
use axum::routing::{get, post};
use axum::{Json, Router};
use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value};
use tower_http::cors::CorsLayer;
use tower_http::services::ServeDir;
use tower_http::trace::TraceLayer;

use crate::core::MantisBase;
use crate::logger::prelude::*;
use crate::models::types::{validate_entity_name, AccessMode, AccessRule, EntityType};
use crate::models::Field;
use crate::storage::Store;

use openapi::build_openapi_value;

#[derive(Clone)]
pub struct AppState {
    pub store: Store,
    pub jwt_secret: Option<String>,
}

pub async fn serve(mantis: &MantisBase, store: Store) -> anyhow::Result<()> {
    let state = Arc::new(AppState {
        store,
        jwt_secret: mantis.jwt_secret().map(|s| s.to_string()),
    });

    let api = Router::new()
        .route("/health", get(health))
        .route("/openapi.json", get(openapi_json))
        .route("/schemas", get(list_schemas).post(create_schema))
        .route("/schemas/{name}", get(get_schema).delete(delete_schema))
        .route("/entities/{entity}", get(list_entities).post(create_row))
        .route(
            "/entities/{entity}/{id}",
            get(get_row).patch(update_row).delete(delete_row),
        )
        .route("/config", get(list_config).patch(patch_config))
        .route("/auth/login", post(auth_login))
        .route("/webhooks", get(list_hooks).post(register_hook))
        .with_state(state.clone());

    let app = Router::new()
        .nest("/api/v1", api)
        .nest_service("/mb", ServeDir::new(mantis.public_dir()))
        .layer(TraceLayer::new_for_http())
        .layer(CorsLayer::permissive())
        .with_state(state);

    let addr = format!("{}:{}", mantis.host(), mantis.port());
    let listener = tokio::net::TcpListener::bind(&addr).await?;
    info!("listening on http://{addr}");
    axum::serve(listener, app).await?;
    Ok(())
}

async fn health() -> Json<Value> {
    Json(json!({ "status": "ok" }))
}

async fn openapi_json(State(state): State<Arc<AppState>>) -> Result<Json<Value>, ApiError> {
    let entities = state.store.list_entities_catalog().await?;
    Ok(Json(build_openapi_value(&entities)))
}

async fn require_admin(headers: &HeaderMap, store: &Store) -> Result<String, ApiError> {
    let raw = headers
        .get(header::AUTHORIZATION)
        .and_then(|v| v.to_str().ok())
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "missing Authorization"))?;
    let b64 = raw
        .strip_prefix("Basic ")
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "expected Basic auth"))?;
    let decoded = base64::Engine::decode(&base64::engine::general_purpose::STANDARD, b64)
        .map_err(|_| ApiError(StatusCode::UNAUTHORIZED, "invalid Basic encoding"))?;
    let s = String::from_utf8(decoded)
        .map_err(|_| ApiError(StatusCode::UNAUTHORIZED, "invalid Basic utf8"))?;
    let (user, pass) = s
        .split_once(':')
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "invalid Basic format"))?;
    let ok = store.verify_admin_basic(user, pass).await?;
    if !ok {
        return Err(ApiError(
            StatusCode::UNAUTHORIZED,
            "invalid admin credentials",
        ));
    }
    Ok(user.to_string())
}

async fn list_schemas(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let v = state.store.list_entities_catalog().await?;
    Ok(Json(json!(v)))
}

#[derive(Debug, Deserialize)]
struct CreateSchemaBody {
    name: String,
    #[serde(rename = "type")]
    entity_type: EntityType,
    #[serde(default)]
    fields: Option<Vec<Field>>,
    #[serde(default)]
    view_sql: Option<String>,
    #[serde(default)]
    rules: Value,
}

async fn create_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<CreateSchemaBody>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    validate_entity_name(&body.name).map_err(ApiError::bad_request)?;
    state
        .store
        .create_entity_from_schema(
            &body.name,
            body.entity_type,
            body.view_sql.as_deref(),
            body.fields,
            &body.rules,
        )
        .await?;
    Ok((
        StatusCode::CREATED,
        Json(json!({ "ok": true, "name": body.name })),
    ))
}

async fn get_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(name): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let v = state
        .store
        .get_entity_catalog(&name)
        .await?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "schema not found"))?;
    Ok(Json(v))
}

async fn delete_schema(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Path(name): Path<String>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    state.store.delete_entity_catalog(&name).await?;
    Ok(Json(json!({ "ok": true })))
}

#[derive(Debug, Deserialize)]
struct ListQuery {
    #[serde(default = "default_limit")]
    limit: u32,
    #[serde(default)]
    offset: u32,
}

fn default_limit() -> u32 {
    50
}

fn bearer_token(headers: &HeaderMap) -> Option<String> {
    let raw = headers.get(header::AUTHORIZATION)?.to_str().ok()?;
    raw.strip_prefix("Bearer ").map(|s| s.to_string())
}

#[derive(Debug, Serialize, Deserialize)]
struct JwtClaims {
    sub: String,
    entity: String,
    exp: u64,
}

async fn check_entity_access(
    state: &AppState,
    entity: &str,
    op: &str,
    headers: &HeaderMap,
) -> Result<(), ApiError> {
    let rule = state
        .store
        .get_access_rule(entity, op)
        .await?
        .unwrap_or_else(AccessRule::admin_only);
    match rule.mode {
        AccessMode::Public => Ok(()),
        AccessMode::Admin => {
            require_admin(headers, &state.store).await?;
            Ok(())
        }
        AccessMode::Authenticated => {
            if require_admin(headers, &state.store).await.is_ok() {
                return Ok(());
            }
            let token = bearer_token(headers).ok_or(ApiError(
                StatusCode::UNAUTHORIZED,
                "Bearer token or admin Basic required",
            ))?;
            let secret = state.jwt_secret.as_deref().ok_or(ApiError(
                StatusCode::INTERNAL_SERVER_ERROR,
                "MB_JWT_SECRET not set",
            ))?;
            let data = jsonwebtoken::decode::<JwtClaims>(
                &token,
                &jsonwebtoken::DecodingKey::from_secret(secret.as_bytes()),
                &jsonwebtoken::Validation::default(),
            )
            .map_err(|_| ApiError(StatusCode::UNAUTHORIZED, "invalid token"))?;
            if data.claims.entity != entity {
                return Err(ApiError(StatusCode::FORBIDDEN, "token entity mismatch"));
            }
            Ok(())
        }
        AccessMode::Custom => Err(ApiError(
            StatusCode::FORBIDDEN,
            "custom access rules not implemented",
        )),
    }
}

async fn list_entities(
    State(state): State<Arc<AppState>>,
    Path(entity): Path<String>,
    Query(q): Query<ListQuery>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "list", &headers).await?;
    let rows = state
        .store
        .list_entity_rows(&entity, q.limit.min(500), q.offset)
        .await?;
    Ok(Json(json!(rows)))
}

async fn get_row(
    State(state): State<Arc<AppState>>,
    Path((entity, id)): Path<(String, String)>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "read", &headers).await?;
    let row = state
        .store
        .get_entity_row(&entity, &id)
        .await?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "row not found"))?;
    Ok(Json(Value::Object(row.into_iter().collect())))
}

async fn create_row(
    State(state): State<Arc<AppState>>,
    Path(entity): Path<String>,
    headers: HeaderMap,
    Json(body): Json<Map<String, Value>>,
) -> Result<(StatusCode, Json<Value>), ApiError> {
    check_entity_access(&state, &entity, "create", &headers).await?;
    let et = state
        .store
        .entity_type(&entity)
        .await?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "entity not found"))?;
    let mut body = body;
    if et == "auth" {
        if let Some(p) = body.get("password").and_then(|v| v.as_str()) {
            let salt = password_hash::SaltString::generate(&mut password_hash::rand_core::OsRng);
            let hash = argon2::Argon2::default()
                .hash_password(p.as_bytes(), &salt)
                .map_err(|_| ApiError::bad_request("password hash failed"))?
                .to_string();
            body.insert("password".into(), json!(hash));
        }
    }
    let row = state.store.insert_entity_row(&entity, body).await?;
    Ok((
        StatusCode::CREATED,
        Json(Value::Object(row.into_iter().collect())),
    ))
}

async fn update_row(
    State(state): State<Arc<AppState>>,
    Path((entity, id)): Path<(String, String)>,
    headers: HeaderMap,
    Json(patch): Json<Map<String, Value>>,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "update", &headers).await?;
    let et = state
        .store
        .entity_type(&entity)
        .await?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "entity not found"))?;
    let mut patch = patch;
    if et == "auth" && patch.contains_key("password") {
        if let Some(p) = patch.get("password").and_then(|v| v.as_str()) {
            let salt = password_hash::SaltString::generate(&mut password_hash::rand_core::OsRng);
            let hash = argon2::Argon2::default()
                .hash_password(p.as_bytes(), &salt)
                .map_err(|_| ApiError::bad_request("password hash failed"))?
                .to_string();
            patch.insert("password".into(), json!(hash));
        }
    }
    let row = state.store.update_entity_row(&entity, &id, patch).await?;
    Ok(Json(Value::Object(row.into_iter().collect())))
}

async fn delete_row(
    State(state): State<Arc<AppState>>,
    Path((entity, id)): Path<(String, String)>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    check_entity_access(&state, &entity, "delete", &headers).await?;
    state.store.delete_entity_row(&entity, &id).await?;
    Ok(Json(json!({ "ok": true })))
}

#[derive(Debug, Deserialize)]
struct AuthLoginBody {
    entity: String,
    identity: String,
    password: String,
}

async fn auth_login(
    State(state): State<Arc<AppState>>,
    Json(body): Json<AuthLoginBody>,
) -> Result<Json<Value>, ApiError> {
    let user = state
        .store
        .verify_auth_login(&body.entity, &body.identity, &body.password)
        .await?
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "invalid credentials"))?;
    let secret = state.jwt_secret.as_deref().ok_or(ApiError(
        StatusCode::INTERNAL_SERVER_ERROR,
        "MB_JWT_SECRET not set",
    ))?;
    let sub = user
        .get("id")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    let exp = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs()
        + 86400 * 7;
    let claims = JwtClaims {
        sub,
        entity: body.entity.clone(),
        exp,
    };
    let token = jsonwebtoken::encode(
        &jsonwebtoken::Header::default(),
        &claims,
        &jsonwebtoken::EncodingKey::from_secret(secret.as_bytes()),
    )
    .map_err(|_| ApiError::internal("jwt encode failed"))?;
    Ok(Json(json!({ "token": token, "user": user })))
}

async fn list_config(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let rows = state.store.app_config_list().await?;
    let mut m = Map::new();
    for (k, v) in rows {
        let parsed: Value = serde_json::from_str(&v).unwrap_or_else(|_| json!(v));
        m.insert(k, parsed);
    }
    Ok(Json(Value::Object(m)))
}

#[derive(Debug, Deserialize)]
struct PatchConfigBody {
    key: String,
    value: Value,
}

async fn patch_config(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<PatchConfigBody>,
) -> Result<Json<Value>, ApiError> {
    let email = require_admin(&headers, &state.store).await?;
    let admins = state.store.list_admins().await?;
    let admin_id = admins
        .iter()
        .find(|(_, em)| *em == email)
        .map(|(id, _)| id.as_str());
    let v =
        serde_json::to_string(&body.value).map_err(|_| ApiError::bad_request("invalid JSON"))?;
    state.store.app_config_set(&body.key, &v, admin_id).await?;
    Ok(Json(json!({ "ok": true })))
}

async fn list_hooks(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let v = state.store.list_webhooks().await?;
    Ok(Json(json!(v)))
}

#[derive(Debug, Deserialize)]
struct RegisterHookBody {
    url: String,
    events: Vec<String>,
    #[serde(default)]
    secret: Option<String>,
}

async fn register_hook(
    State(state): State<Arc<AppState>>,
    headers: HeaderMap,
    Json(body): Json<RegisterHookBody>,
) -> Result<Json<Value>, ApiError> {
    let _ = require_admin(&headers, &state.store).await?;
    let id = state
        .store
        .register_webhook(&body.url, &body.events, body.secret.as_deref())
        .await?;
    Ok(Json(json!({ "id": id })))
}

#[derive(Debug, Clone, Copy)]
pub struct ApiError(StatusCode, &'static str);

impl ApiError {
    fn bad_request(msg: &'static str) -> Self {
        ApiError(StatusCode::BAD_REQUEST, msg)
    }
    fn internal(msg: &'static str) -> Self {
        ApiError(StatusCode::INTERNAL_SERVER_ERROR, msg)
    }
}

impl IntoResponse for ApiError {
    fn into_response(self) -> axum::response::Response {
        let body = json!({ "error": self.1 });
        (self.0, Json(body)).into_response()
    }
}

impl From<crate::storage::StorageError> for ApiError {
    fn from(e: crate::storage::StorageError) -> Self {
        match e {
            crate::storage::StorageError::NotFound(_) => {
                ApiError(StatusCode::NOT_FOUND, "not found")
            }
            crate::storage::StorageError::Conflict(_) => ApiError(StatusCode::CONFLICT, "conflict"),
            crate::storage::StorageError::Validation(_) => {
                ApiError(StatusCode::BAD_REQUEST, "validation failed")
            }
            #[cfg(feature = "postgres")]
            crate::storage::StorageError::SqlxMigrate(_) => {
                ApiError(StatusCode::INTERNAL_SERVER_ERROR, "migration error")
            }
            _ => ApiError(StatusCode::INTERNAL_SERVER_ERROR, "storage error"),
        }
    }
}
