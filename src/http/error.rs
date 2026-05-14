use axum::http::StatusCode;
use axum::response::IntoResponse;
use axum::Json;
use serde_json::json;

#[derive(Debug, Clone, Copy)]
pub struct ApiError(pub StatusCode, pub &'static str);

impl ApiError {
    pub fn bad_request(msg: &'static str) -> Self {
        ApiError(StatusCode::BAD_REQUEST, msg)
    }
    pub fn internal(msg: &'static str) -> Self {
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
            crate::storage::StorageError::SqlxMigrate(_) => {
                ApiError(StatusCode::INTERNAL_SERVER_ERROR, "migration error")
            }
            _ => ApiError(StatusCode::INTERNAL_SERVER_ERROR, "storage error"),
        }
    }
}
