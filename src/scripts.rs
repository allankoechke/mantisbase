//! Lean sandboxed JS (`embed-js` feature). Hooks are intentionally minimal.

#[cfg(feature = "embed-js")]
pub fn eval_hook(_source: &str) -> Result<String, &'static str> {
    // Wire `rquickjs` with strict fuel/timeout in a dedicated follow-up.
    Err("embed-js runtime not yet wired")
}

#[cfg(not(feature = "embed-js"))]
pub fn eval_hook(_source: &str) -> Result<String, &'static str> {
    Err("rebuild with `--features embed-js` (experimental)")
}
