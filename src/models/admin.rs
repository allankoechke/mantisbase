//! Built-in administrator row (`mb_admin`).

use uuid::Uuid;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AdminRow {
    pub id: String,
    pub email: String,
    pub active: bool,
    pub password_reset_required: bool,
}

/// Trims surrounding ASCII whitespace, then checks a practical email shape (ASCII-oriented).
pub fn normalize_and_validate_admin_email(raw: &str) -> Result<String, &'static str> {
    let email = raw.trim_matches(|c: char| c.is_ascii_whitespace());
    if email.is_empty() {
        return Err("email must not be empty");
    }
    if email.len() > 254 {
        return Err("email is too long");
    }
    if email.chars().any(char::is_whitespace) {
        return Err("email must not contain whitespace");
    }
    let Some((local, domain)) = email.split_once('@') else {
        return Err("invalid email");
    };
    if local.is_empty() || local.len() > 64 {
        return Err("invalid email");
    }
    if domain.is_empty() || !domain.contains('.') {
        return Err("invalid email");
    }
    if domain.starts_with('.') || domain.ends_with('.') || domain.contains("..") {
        return Err("invalid email");
    }
    let local_ok = local
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '_' | '%' | '+' | '-'));
    let domain_ok = domain
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || matches!(c, '.' | '-'));
    if !local_ok || !domain_ok {
        return Err("invalid email");
    }
    Ok(email.to_string())
}

/// Trims surrounding ASCII whitespace; password must be at least 8 characters after trim.
pub fn normalize_and_validate_admin_password(raw: &str) -> Result<String, &'static str> {
    let password = raw.trim_matches(|c: char| c.is_ascii_whitespace());
    if password.len() < 8 {
        return Err("password must be at least 8 characters");
    }
    Ok(password.to_string())
}

/// For `DELETE` / `--rm`: trim, then require either a valid admin email or a UUID row id.
pub fn normalize_and_validate_admin_remove_target(raw: &str) -> Result<String, &'static str> {
    let s = raw.trim_matches(|c: char| c.is_ascii_whitespace());
    if s.is_empty() {
        return Err("admin id or email must not be empty");
    }
    if s.contains('@') {
        return normalize_and_validate_admin_email(s);
    }
    Uuid::parse_str(s).map_err(|_| "admin id must be a valid UUID")?;
    Ok(s.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn email_accepts_common_form() {
        assert!(normalize_and_validate_admin_email("  a@b.co  ").unwrap() == "a@b.co");
    }

    #[test]
    fn email_rejects_no_at() {
        assert!(normalize_and_validate_admin_email("ab.co").is_err());
    }

    #[test]
    fn password_requires_length_after_trim() {
        assert!(normalize_and_validate_admin_password("  abcdefgh  ").is_ok());
        assert!(normalize_and_validate_admin_password(" abcdefg ").is_err());
    }

    #[test]
    fn remove_target_accepts_uuid() {
        let u = "550e8400-e29b-41d4-a716-446655440000";
        assert_eq!(normalize_and_validate_admin_remove_target(u).unwrap(), u);
    }
}
