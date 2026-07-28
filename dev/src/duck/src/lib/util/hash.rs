//! Our implementation(s) of various hash functions.

use sha2::{Digest, Sha256};

/// Calculate SHA-256 hash and return it as an array of bytes.
pub fn sha256_bytes<H: AsRef<[u8]>>(data: H) -> [u8; 32] {
    let mut sha256 = Sha256::new();
    sha256.update(data);
    let hash = sha256.finalize();
    let mut result = [0; 32];
    result.copy_from_slice(&hash);
    result
}

/// Helper for [`sha256_bytes`], but returns a hex encoded string.
pub fn sha256_string<H: AsRef<[u8]>>(data: H) -> String {
    let hash = sha256_bytes(data);
    super::hex::encode(hash)
}

/// Struct for hashing long series of words.
pub struct Sha256Hasher {
    /// Hash of the currently seen words.
    current_sha256: Sha256,
    /// Additional string hashed after every update.
    separator: &'static str,
}

impl Sha256Hasher {
    /// Create a new [`Sha256Hasher`].
    pub fn new(separator: &'static str) -> Self {
        Self {
            current_sha256: Sha256::new(),
            separator,
        }
    }

    /// Update the current hash by one word.
    pub fn update<H: AsRef<[u8]>>(&mut self, data: H) {
        self.current_sha256.update(data);
        self.current_sha256.update(self.separator);
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
