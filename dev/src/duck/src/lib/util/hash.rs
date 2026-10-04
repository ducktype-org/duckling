//! Our implementation(s) of various hash functions.

use sha2::{Digest, Sha256};

/// Calculate SHA-256 hash and return it as an array of bytes.
pub fn sha256_bytes<H: AsRef<[u8]>>(data: H) -> [u8; 32] {
    let mut hasher = Sha256Hasher::new(None);
    hasher.update(data);
    hasher.into_bytes()
}

/// Helper for [`sha256_bytes`], but returns a hex encoded string.
pub fn sha256_string<H: AsRef<[u8]>>(data: H) -> String {
    let mut hasher = Sha256Hasher::new(None);
    hasher.update(data);
    hasher.into_string()
}

/// Struct for hashing long series of words.
pub struct Sha256Hasher {
    /// Hash of the currently seen words.
    current_sha256: Sha256,
    /// Additional string hashed after every update.
    separator: Option<&'static str>,
}

impl Sha256Hasher {
    /// Create a new [`Sha256Hasher`].
    pub fn new(separator: Option<&'static str>) -> Self {
        Self {
            current_sha256: Sha256::new(),
            separator,
        }
    }

    /// Update the current hash by one word.
    pub fn update<H: AsRef<[u8]>>(&mut self, data: H) {
        self.current_sha256.update(data);
        if let Some(separator) = self.separator {
            self.current_sha256.update(separator);
        }
    }

    /// Finalize hashing and return result as an array of bytes.
    pub fn into_bytes(self) -> [u8; 32] {
        let hash = self.current_sha256.finalize();
        let mut result = [0; 32];
        result.copy_from_slice(&hash);
        result
    }

    /// Finalize hashing and return result as a [`String`].
    pub fn into_string(self) -> String {
        let hash = self.into_bytes();
        super::hex::encode(hash)
    }
}
