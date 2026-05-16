//! Per-request access logging at `info` level (spdlog via [`crate::logger`]).

use std::time::Instant;

use axum::body::{to_bytes, Body};
use axum::extract::ConnectInfo;
use axum::http::{header, Request, Response, Version};
use axum::middleware::Next;

use crate::logger::prelude::*;

const ERR_BODY_LOG_MAX: usize = 512;
const ERR_BODY_READ_MAX: usize = 4096;

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

fn clip_body_for_log(bytes: &[u8]) -> String {
    let mut s = String::from_utf8_lossy(bytes).replace(['\n', '\r'], " ");
    if s.len() > ERR_BODY_LOG_MAX {
        s.truncate(ERR_BODY_LOG_MAX);
        s.push('…');
    }
    s
}

/// One structured line per completed request (API and static `/mb`).
pub async fn log_request(req: Request<Body>, next: Next) -> Response<Body> {
    let method = req.method().clone();
    let resource = req
        .uri()
        .path_and_query()
        .map(|pq| pq.as_str().to_string())
        .unwrap_or_else(|| req.uri().path().to_string());
    let version = req.version();
    let peer = req
        .extensions()
        .get::<ConnectInfo<std::net::SocketAddr>>()
        .map(|c| c.0.to_string())
        .unwrap_or_else(|| "-".to_string());

    let start = Instant::now();
    let mut response = next.run(req).await;
    let elapsed_ms = start.elapsed().as_secs_f64() * 1000.0;
    let status = response.status();

    let err_body = if status.is_success() {
        String::new()
    } else {
        let (mut parts, body) = response.into_parts();
        let (body, logged) = match to_bytes(body, ERR_BODY_READ_MAX).await {
            Ok(b) => {
                let logged = clip_body_for_log(&b);
                parts.headers.remove(header::CONTENT_LENGTH);
                (Body::from(b), logged)
            }
            Err(_) => (Body::empty(), "(response body unreadable)".into()),
        };
        response = Response::from_parts(parts, body);
        logged
    };

    if status.is_success() {
        info!(
            "{}  {:<8}  {:<7}  {}  {}  {:.3} ms",
            peer,
            http_version_label(version),
            method,
            status.as_u16(),
            resource,
            elapsed_ms
        );
    } else {
        warn!(
            "{}  {:<8}  {:<7}  {}  {}  {:.3} ms  \n\t└──  {}",
            peer,
            http_version_label(version),
            method,
            status.as_u16(),
            resource,
            elapsed_ms,
            err_body
        );
    }

    response
}
