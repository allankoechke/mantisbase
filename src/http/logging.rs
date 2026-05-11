//! Per-request access logging at `info` level (spdlog via [`crate::logger`]).

use std::time::Instant;

use axum::body::Body;
use axum::extract::ConnectInfo;
use axum::http::{header, Request, Response, Version};
use axum::middleware::Next;

use crate::logger::prelude::*;

fn http_version_str(v: Version) -> &'static str {
    match v {
        Version::HTTP_09 => "HTTP/0.9",
        Version::HTTP_10 => "HTTP/1.0",
        Version::HTTP_11 => "HTTP/1.1",
        Version::HTTP_2 => "HTTP/2",
        Version::HTTP_3 => "HTTP/3",
        _ => "HTTP/?",
    }
}

/// Logs one line per request: method, URI, protocol, status, duration, peer, optional lengths.
pub async fn log_request(req: Request<Body>, next: Next) -> Response<Body> {
    let method = req.method().clone();
    let uri = req.uri().clone();
    let version = req.version();
    let peer = req
        .extensions()
        .get::<ConnectInfo<std::net::SocketAddr>>()
        .map(|c| c.0.to_string())
        .unwrap_or_else(|| "-".to_string());
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
    let elapsed = start.elapsed();
    let status = response.status();
    let resp_clen = response
        .headers()
        .get(header::CONTENT_LENGTH)
        .and_then(|v| v.to_str().ok())
        .map(String::from)
        .unwrap_or_else(|| "-".to_string());

    info!(
        "{} \"{}\" {} -> {} {:.3}ms peer={} user_agent=\"{}\" req_content_length={} resp_content_length={}",
        method,
        uri,
        http_version_str(version),
        status.as_u16(),
        elapsed.as_secs_f64() * 1000.0,
        peer,
        user_agent.replace('"', "'"),
        req_clen,
        resp_clen
    );

    response
}
