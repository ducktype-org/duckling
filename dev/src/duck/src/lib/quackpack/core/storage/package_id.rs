use std::path::{Path, PathBuf};

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
    id: StrId,
    version: Version,
    url: Url,
}

impl RegistryId {
    pub fn new(id: StrId, version: Version, url: Url) -> Self {
        Self { id, version, url }
    }

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

    pub fn id(&self) -> StrId {
        self.id
    }

    pub fn set_id(&mut self, id: StrId) {
        self.id = id;
    }

    pub fn version(&self) -> Version {
        self.version
    }

    pub fn version_mut(&mut self) -> &mut Version {
        &mut self.version
    }

    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }

    pub fn url(&self) -> &Url {
        &self.url
    }

    pub fn url_mut(&mut self) -> &mut Url {
        &mut self.url
    }

    pub fn set_url(&mut self, url: Url) {
        self.url = url;
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
pub struct GitId {
    url: Url,
    commit: StrId,
}

impl GitId {
    pub fn new(url: Url, commit: StrId) -> Self {
        Self { url, commit }
    }

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

    pub fn url(&self) -> &Url {
        &self.url
    }

    pub fn url_mut(&mut self) -> &mut Url {
        &mut self.url
    }

    pub fn set_url(&mut self, url: Url) {
        self.url = url;
    }

    pub fn commit(&self) -> StrId {
        self.commit
    }

    pub fn set_commit(&mut self, commit: StrId) {
        self.commit = commit;
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
pub struct LocalId {
    path: PathBuf,
}

impl LocalId {
    pub fn new(path: PathBuf) -> Self {
        Self { path }
    }

    pub const TYPE: &str = "local";

    pub fn storage_name(&self) -> StrId {
        format!(
            "{}-{}",
            Self::TYPE,
            sha256_string(self.path.as_os_str().as_encoded_bytes())
        )
        .into()
    }

    pub fn path(&self) -> &Path {
        self.path.as_path()
    }

    pub fn set_path(&mut self, path: PathBuf) {
        self.path = path;
    }

    pub fn path_mut(&mut self) -> &mut PathBuf {
        &mut self.path
    }
}
