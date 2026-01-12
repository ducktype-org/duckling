use crate::{
    StrId,
    quackpack::{core::Version, schemas::registry},
};

use serde::{Deserialize, Serialize};

use url::Url;

use crate::quackpack::core;

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct PackageWithUrl {
    pub id: StrId,
    pub version: Version,
    pub url: Url,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Package {
    pub id: StrId,
    pub version: Version,
}

impl From<&PackageWithUrl> for Package {
    fn from(value: &PackageWithUrl) -> Self {
        Self {
            id: value.id,
            version: value.version,
        }
    }
}

impl From<PackageWithUrl> for Package {
    fn from(value: PackageWithUrl) -> Self {
        Self::from(&value)
    }
}

impl From<&registry::Manifest> for Package {
    fn from(value: &registry::Manifest) -> Self {
        let name = &value.metadata.name;
        Self {
            id: name.into(),
            version: value.metadata.version,
        }
    }
}

#[derive(Debug)]
pub struct GitCloneResponse {
    pub commit_hash: StrId,
    pub package: core::Package,
}

#[derive(Debug, Deserialize, Serialize)]
pub struct MultiMetadata {
    pub packages_metadata: Vec<registry::Manifest>,
}

#[derive(Debug, Deserialize)]
pub struct SearchResult {
    pub result: Vec<Package>,
}
