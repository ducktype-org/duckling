use crate::{StrId, quackpack::core::Version};

use serde::{Deserialize, Serialize};

use url::Url;

use crate::quackpack::core;

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Package {
    pub id: StrId,
    pub version: Version,
    pub url: Url,
}

#[derive(Debug)]
pub struct GitCloneResponse {
    pub commit_hash: StrId,
    pub package: core::Package,
}
