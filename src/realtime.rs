//! Realtime change feeds (SSE / WebSockets) — implemented per-backend after baseline features.
//!
//! Design: a `RealtimeSource` trait hides libSQL vs Postgres vs Turso notify strategies; transport
//! (SSE vs WebSocket) is selected via `mb_app_config` (`realtime.transport`).

/// Placeholder for future realtime integration (see plan §13).
pub trait RealtimeSource: Send + Sync {}
