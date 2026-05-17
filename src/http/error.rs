use axum::http::StatusCode;
use axum::response::IntoResponse;
use axum::Json;
use serde_json::json;

#[derive(Debug, Clone)]
pub struct ApiError(pub StatusCode, pub String);

impl ApiError {
    pub fn new(status: StatusCode, msg: impl Into<String>) -> Self {
        Self(status, msg.into())
    }

    pub fn bad_request(msg: impl Into<String>) -> Self {
        Self(StatusCode::BAD_REQUEST, msg.into())
    }

    pub fn not_found(msg: impl Into<String>) -> Self {
        Self(StatusCode::NOT_FOUND, msg.into())
    }

    pub fn internal(msg: impl Into<String>) -> Self {
        Self(StatusCode::INTERNAL_SERVER_ERROR, msg.into())
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
            crate::storage::StorageError::NotFound(msg) => ApiError::not_found(msg),
            crate::storage::StorageError::Conflict(msg) => {
                ApiError::new(StatusCode::CONFLICT, msg)
            }
            crate::storage::StorageError::Validation(msg) => ApiError::bad_request(msg),
            crate::storage::StorageError::SqlxMigrate(_) => {
                ApiError::internal("migration error")
            }
            _ => ApiError::internal("storage error"),
        }
    }
}
