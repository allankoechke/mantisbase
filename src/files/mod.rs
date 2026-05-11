//! Local filesystem object storage (`FileStorage`).

use std::path::{Path, PathBuf};

use sha2::{Digest, Sha256};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum FileError {
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
    #[error("invalid storage key")]
    InvalidKey,
}

pub type Result<T> = std::result::Result<T, FileError>;

/// Abstract storage for uploaded blobs (v1: [`LocalFs`] only).
pub trait FileStorage: Send + Sync {
    fn put(&self, bytes: &[u8]) -> Result<String>;
    fn get_path(&self, storage_key: &str) -> Result<PathBuf>;
}

/// Content-addressed files under `data_dir/files/sha256/ab/...`.
pub struct LocalFs {
    root: PathBuf,
}

impl LocalFs {
    pub fn new(data_dir: impl AsRef<Path>) -> Result<Self> {
        let root = data_dir.as_ref().join("files");
        std::fs::create_dir_all(&root)?;
        Ok(Self { root })
    }

    fn key_to_rel_path(key: &str) -> Result<PathBuf> {
        let key = key.trim();
        if key.len() != 64 || !key.chars().all(|c| c.is_ascii_hexdigit()) {
            return Err(FileError::InvalidKey);
        }
        let a = &key[0..2];
        let b = &key[2..4];
        let body = &key[4..];
        Ok(PathBuf::from("sha256").join(a).join(b).join(body))
    }
}

impl FileStorage for LocalFs {
    fn put(&self, bytes: &[u8]) -> Result<String> {
        let hash = hex::encode(Sha256::digest(bytes));
        let rel = Self::key_to_rel_path(&hash)?;
        let path = self.root.join(&rel);
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        if !path.exists() {
            std::fs::write(&path, bytes)?;
        }
        Ok(hash)
    }

    fn get_path(&self, storage_key: &str) -> Result<PathBuf> {
        let rel = Self::key_to_rel_path(storage_key)?;
        Ok(self.root.join(rel))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn local_fs_put_get() {
        let dir = tempfile::tempdir().unwrap();
        let fs = LocalFs::new(dir.path()).unwrap();
        let key = fs.put(b"hello").unwrap();
        let p = fs.get_path(&key).unwrap();
        assert_eq!(std::fs::read(p).unwrap(), b"hello");
    }
}
