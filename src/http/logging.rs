//! Per-request access logging at `info` level (spdlog via [`crate::logger`]).

use std::time::Instant;

use axum::body::Body;
use axum::extract::ConnectInfo;
use axum::http::{header, Request, Response, Version};
use axum::middleware::Next;

use crate::logger::prelude::*;

fn http_version_label(v: Version) -> &'static str {
    match v {
        Version::HTTP_09 => "HTTP/0.9",
        Version::HTTP_10 => "HTTP/1.0",
        Version::HTTP_11 => "HTTP/1.1",
        Version::HTTP_2 => "HTTP/2",
        Version::HTTP_3 => "HTTP/3",
        _ => "HTTP/?",
    }
}

fn sanitize_ua(ua: &str) -> String {
    const MAX_CHARS: usize = 160;
    let u = ua.replace('"', "'");
    if u.chars().count() > MAX_CHARS {
        u.chars().take(MAX_CHARS).collect::<String>() + "…"
    } else {
        u
    }
}

/// One structured `info!` line per completed request (API and static `/mb`).
pub async fn log_request(req: Request<Body>, next: Next) -> Response<Body> {
    let method = req.method().clone();
    let uri = req.uri().clone();
    let version = req.version();
    let peer = req
        .extensions()
        .get::<ConnectInfo<std::net::SocketAddr>>()
        .map(|c| c.0.to_string())
        .unwrap_or_else(|| "-".to_string());
    let resource = uri
        .path_and_query()
        .map(|pq| pq.as_str().to_string())
        .unwrap_or_else(|| uri.path().to_string());
    let req_clen = req
        .headers()
        .get(header::CONTENT_LENGTH)
        .and_then(|v| v.to_str().ok())
        .map(String::from)
        .unwrap_or_else(|| "-".to_string());
    let user_agent = req
        .headers()
        .get(header::USER_AGENT)
        .and_then(|v| v.to_str().ok())
        .map(String::from)
        .unwrap_or_else(|| "-".to_string());

    let start = Instant::now();
    let response = next.run(req).await;
    let elapsed_ms = start.elapsed().as_secs_f64() * 1000.0;
    let status = response.status();
    let resp_clen = response
        .headers()
        .get(header::CONTENT_LENGTH)
        .and_then(|v| v.to_str().ok())
        .map(String::from)
        .unwrap_or_else(|| "-".to_string());

    // LogOrigin::info("HTTP Request", fmt::format("{} {:<7} {}  - Status: {}  - Time: {}ms\n\t└──Body: {}",
    // req.version, req.method, req.path, res.status, duration_ms, body));

    if status.is_success() {
        info!(
            "{}   {:<8}  {:<7}  {}  -  Status: {}  -  Time: {:.3} ms",
            peer,
            http_version_label(version),
            method,
            resource,
            status.as_u16(),
            elapsed_ms
        );
    } else {
        warn!(
            "{}   {:<8}  {:<7}  {}  -  Status: {}  -  Time: {:.3} ms\n\t└──Body: {:?}",
            peer,
            http_version_label(version),
            method,
            resource,
            status.as_u16(),
            elapsed_ms,
            response.body()
        );
    }

    response
}
