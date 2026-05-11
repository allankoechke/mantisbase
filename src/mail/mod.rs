//! SMTP mail (optional `smtp` feature).

#[cfg(feature = "smtp")]
mod smtp_impl;
#[cfg(feature = "smtp")]
pub use smtp_impl::*;

#[cfg(not(feature = "smtp"))]
/// Send a plain-text email when the `smtp` feature is enabled.
pub async fn send_text_email(_to: &str, _subject: &str, _body: &str) -> Result<(), &'static str> {
    Err("mantisbase built without `smtp` feature")
}
