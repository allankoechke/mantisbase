use lettre::message::{Mailbox, Message, SinglePart};
use lettre::transport::smtp::authentication::Credentials;
use lettre::{AsyncSmtpTransport, AsyncTransport, Tokio1Executor};

/// Minimal helper using `MB_SMTP_*` environment variables.
pub async fn send_text_email(to: &str, subject: &str, body: &str) -> Result<(), String> {
    let host = std::env::var("MB_SMTP_HOST").map_err(|_| "MB_SMTP_HOST".to_string())?;
    let user = std::env::var("MB_SMTP_USER").unwrap_or_default();
    let pass = std::env::var("MB_SMTP_PASSWORD").unwrap_or_default();
    let from = std::env::var("MB_SMTP_FROM").map_err(|_| "MB_SMTP_FROM".to_string())?;

    let email = Message::builder()
        .from(from.parse::<Mailbox>().map_err(|e| e.to_string())?)
        .to(to.parse::<Mailbox>().map_err(|e| e.to_string())?)
        .subject(subject)
        .singlepart(SinglePart::plain(body.to_string()))
        .map_err(|e| e.to_string())?;

    let creds = Credentials::new(user, pass);
    let mailer = AsyncSmtpTransport::<Tokio1Executor>::relay(&host)
        .map_err(|e| e.to_string())?
        .credentials(creds)
        .build();

    mailer.send(email).await.map_err(|e| e.to_string())?;
    Ok(())
}
