//! Resolve default-style relative paths against the running executable (not the process cwd).

use std::path::{Path, PathBuf};

/// Parent directory of the running executable, used as the anchor for relative app paths.
/// Falls back to [`std::env::current_dir`], then `"."`, if the executable path is unavailable.
pub fn binary_parent_dir() -> PathBuf {
    std::env::current_exe()
        .ok()
        .and_then(|exe| exe.parent().map(Path::to_path_buf))
        .or_else(|| std::env::current_dir().ok())
        .unwrap_or_else(|| PathBuf::from("."))
}

/// If `path` is absolute, returns it unchanged. If empty, returns it unchanged. Otherwise joins
/// [`binary_parent_dir`].
pub fn resolve_relative_to_binary(path: &Path) -> PathBuf {
    if path.as_os_str().is_empty() {
        return path.to_path_buf();
    }
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        binary_parent_dir().join(path)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn absolute_path_unchanged() {
        let p = if cfg!(windows) {
            PathBuf::from(r"C:\tmp\mantis")
        } else {
            PathBuf::from("/tmp/mantis")
        };
        assert_eq!(resolve_relative_to_binary(&p), p);
    }
}
