use std::path::PathBuf;

use serde::{Deserialize, Serialize};
use url::Url;

use crate::{StrId, quackpack::core::Version, util_common::hash::sha256_string};

#[derive(Debug, Deserialize, Serialize, Clone, Hash, PartialEq, Eq)]
pub enum PackageId {
    Registry(RegistryId),
    Git(GitId),
    Local(LocalId),
}

impl PackageId {
    pub fn is_registry(&self) -> bool {
        matches!(self, PackageId::Registry(..))
    }

    pub fn is_git(&self) -> bool {
        matches!(self, PackageId::Git(..))
    }

    pub fn is_local(&self) -> bool {
        matches!(self, PackageId::Local(..))
    }

    pub fn storage_name(&self) -> StrId {
        match self {
            Self::Registry(registry_id) => registry_id.storage_name(),
            Self::Git(git_id) => git_id.storage_name(),
            Self::Local(local_id) => local_id.storage_name(),
        }
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
pub struct RegistryId {
    pub id: StrId,
    pub version: Version,
    pub url: Url,
}

impl RegistryId {
    pub const TYPE: &str = "registry";
    pub fn storage_name(&self) -> StrId {
        format!(
            "{}-{}-{}-{}",
            Self::TYPE,
            self.url.host_str().unwrap_or_default(),
            self.id,
            self.version
        )
        .into()
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
pub struct GitId {
    pub url: Url,
    pub commit: StrId,
}

impl GitId {
    pub const TYPE: &str = "git";

    pub fn storage_name(&self) -> StrId {
        format!(
            "{}-{}-{}",
            Self::TYPE,
            sha256_string(self.url.as_str()),
            self.commit
        )
        .into()
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
pub struct LocalId {
    pub path: PathBuf,
}

impl LocalId {
    pub const TYPE: &str = "local";

    pub fn storage_name(&self) -> StrId {
        format!(
            "{}-{}",
            Self::TYPE,
            sha256_string(self.path.as_os_str().as_encoded_bytes())
        )
        .into()
    }
}
