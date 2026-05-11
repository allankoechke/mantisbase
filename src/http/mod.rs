//! Axum HTTP server: `/api/v1/*`, admin Basic auth, OpenAPI, static `/mb`.

pub mod openapi;

mod auth;
mod entities;
mod error;
mod jwt;
mod logging;
mod login;
mod meta;
mod schemas;
mod settings;
mod webhooks;

pub use error::ApiError;

use std::net::SocketAddr;
use std::sync::Arc;

use axum::middleware;
use axum::routing::{get, post};
use axum::Router;
use tower_http::cors::CorsLayer;
use tower_http::services::ServeDir;

use crate::core::MantisBase;
use crate::logger::cli_stdout_line;
use crate::storage::Store;

/// Shared HTTP state (store + JWT secret for [`login::auth_login`]).
#[derive(Clone)]
pub struct AppState {
    pub store: Store,
    pub jwt_secret: Option<String>,
}

/// Builds the `/api/v1` router with all JSON routes.
fn api_router(state: Arc<AppState>) -> Router {
    Router::new()
        .route("/health", get(meta::health))
        .route("/openapi.json", get(meta::openapi_json))
        .route(
            "/schemas",
            get(schemas::list_schemas).post(schemas::create_schema),
        )
        .route(
            "/schemas/{name}",
            get(schemas::get_schema).delete(schemas::delete_schema),
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
        .route(
            "/config",
            get(settings::list_config).patch(settings::patch_config),
        )
        .route("/auth/login", post(login::auth_login))
        .route(
            "/webhooks",
            get(webhooks::list_hooks).post(webhooks::register_hook),
        )
        .with_state(state)
}

/// Binds the listener and serves HTTP until shutdown.
pub async fn serve(mantis: &MantisBase, store: Store) -> anyhow::Result<()> {
    let state = Arc::new(AppState {
        store,
        jwt_secret: mantis.jwt_secret().map(|s| s.to_string()),
    });

    let api = api_router(state.clone());

    let app = Router::new()
        .nest("/api/v1", api)
        .nest_service("/mb", ServeDir::new(mantis.public_dir()))
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
