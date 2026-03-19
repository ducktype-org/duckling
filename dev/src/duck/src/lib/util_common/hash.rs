//! Our implementation(s) of various hash functions.

use sha2::{Digest, Sha256};

use crate::StrId;

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
pub fn sha256_string<H: AsRef<[u8]>>(data: H) -> StrId {
    let hash = sha256_bytes(data);
    super::hex::encode(hash).into()
}
