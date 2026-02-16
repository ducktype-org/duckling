use sha2::{Digest, Sha256};

use crate::StrId;

pub fn sha256_bytes<H: AsRef<[u8]>>(data: H) -> [u8; 32] {
    let mut sha256 = Sha256::new();
    sha256.update(data);
    let hash = sha256.finalize();
    let mut result = [0; 32];
    result.copy_from_slice(&hash);
    result
}

pub fn sha256_string<H: AsRef<[u8]>>(data: H) -> StrId {
    let hash = sha256_bytes(data);
    hex::encode(hash).into()
}
