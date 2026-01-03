use crate::{StrId, quackpack::core::Version};

use serde::{Deserialize, Serialize};

use crate::quackpack::core;

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Package {
    pub id: StrId,
    pub version: Version,
    // @TODO: #1353 Do we want to port something safer, like <https://docs.rs/url/latest/url/>?
    pub url: StrId,
}

#[derive(Debug)]
pub struct GitCloneResponse {
    pub commit_hash: StrId,
    pub package: core::Package,
}
