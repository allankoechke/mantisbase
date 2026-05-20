//! Axum HTTP server: `/api/v1/*`, admin **Basic** or **admin JWT** on `/api/v1/sys/*` and `/api/v1/admins/*`, OpenAPI, compiled admin UI at `/mb`.

pub mod openapi;

mod admins;
mod auth;
mod entities;
mod error;
mod jwt;
mod logging;
mod login;
mod meta;
mod schemas;
mod settings;
mod setup;
mod sys;
mod webhooks;

pub use error::ApiError;

use std::net::SocketAddr;

use std::sync::Arc;

use axum::middleware;
use axum::routing::{get, post};
use axum::Router;
use tower_http::cors::CorsLayer;
use tower_http::services::ServeDir;

use std::path::Path;

use crate::core::MantisBase;
use crate::logger::cli_stdout_line;
use crate::logger::prelude::*;
use crate::storage::Store;

/// Shared HTTP state (store + JWT secret for [`login::auth_login`] and [`admins::admin_auth_login`]).
#[derive(Clone)]
pub struct AppState {
    pub store: Store,
    pub jwt_secret: Option<String>,
    /// HMAC secret for JWTs this process (from `MB_JWT_SECRET` or ephemeral).
    pub signing_key: String,
    pub setup: Arc<setup::SetupState>,
    pub skip_setup: bool,
}

/// Builds the `/api/v1` router with all JSON routes.
fn api_router(state: Arc<AppState>) -> Router {
    Router::new()
        .route("/health", get(meta::health))
        .route("/openapi.json", get(meta::openapi_json))
        .route(
            "/sys/schemas",
            get(schemas::list_schemas).post(schemas::create_schema),
        )
        .route(
            "/sys/schemas/{name}",
            get(schemas::get_schema)
                .patch(schemas::patch_schema)
                .delete(schemas::delete_schema),
        )
        .route(
            "/sys/configs",
            get(settings::list_config).patch(settings::patch_config),
        )
        .route("/sys/logs", get(sys::get_logs))
        .route("/admins/auth/login", post(admins::admin_auth_login))
        .route(
            "/admins",
            get(admins::list_admins).post(admins::create_admin),
        )
        .route(
            "/admins/{id}",
            get(admins::get_admin).delete(admins::delete_admin),
        )
        .route(
            "/entities/{entity}",
            get(entities::list_entities).post(entities::create_row),
        )
        .route(
            "/entities/{entity}/{id}",
            get(entities::get_row)
                .patch(entities::update_row)
                .delete(entities::delete_row),
        )
        .route("/auth/login", post(login::auth_login))
        .route("/setup/status", get(setup::setup_status))
        .route("/setup/first-admin", post(setup::create_first_admin))
        .route(
            "/webhooks",
            get(webhooks::list_hooks).post(webhooks::register_hook),
        )
        .with_state(state)
}

/// Binds the listener and serves HTTP until shutdown.
pub async fn serve(mantis: &MantisBase, store: Store, skip_setup: bool) -> anyhow::Result<()> {
    let signing_key = setup::signing_key_for_process(mantis.jwt_secret());
    let state = Arc::new(AppState {
        jwt_secret: mantis.jwt_secret().map(|s| s.to_string()),
        store,
        signing_key,
        setup: Arc::new(setup::SetupState::default()),
        skip_setup,
    });

    setup::bootstrap_first_admin_setup(mantis.host(), mantis.port(), &state).await?;

    let api = api_router(state.clone());

    let admin_root = mantis.admin_ui_dir();
    if !Path::new(admin_root).join("index.html").is_file() {
        warn!(
            "Admin UI not found at {admin_root}/index.html — run: cd admin && npm install && npm run build (writes public/mb-dist/)"
        );
    }

    let app = Router::new()
        .nest("/api/v1", api)
        // .route("/mb/setup", get(setup::setup_page))
        .nest_service("/mb", ServeDir::new(admin_root))
        .layer(CorsLayer::permissive())
        .layer(middleware::from_fn(logging::log_request));

    let addr = format!("{}:{}", mantis.host(), mantis.port());
    let listener = tokio::net::TcpListener::bind(&addr).await?;
    cli_stdout_line(format!(
        "\nStarting Servers: \n\t├── API Endpoints: http://{addr}/api/v1"
    ));
    cli_stdout_line(format!("\t└── Admin Dashboard: http://{addr}/mb\n"));
    axum::serve(
        listener,
        app.into_make_service_with_connect_info::<SocketAddr>(),
    )
    .await?;
    Ok(())
}
