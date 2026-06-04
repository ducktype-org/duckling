use serde::{Deserialize, Serialize};

use crate::StrId;
use crate::quackpack::core::Version;
use crate::quackpack::util::interned_url::InternedUrl;
use crate::util::hash::sha256_string;

#[derive(Debug, Deserialize, Serialize, Clone, Hash, PartialEq, Eq)]
/// An ID of a stored package.
pub enum PackageId {
    /// A registry package.
    Registry(RegistryId),
    /// A git package.
    Git(GitId),
    /// A local package.
    Local(LocalId),
}

impl PackageId {
    /// Whether this is a registry package.
    pub fn is_registry(&self) -> bool {
        matches!(self, PackageId::Registry(..))
    }

    /// Whether this is a git package.
    pub fn is_git(&self) -> bool {
        matches!(self, PackageId::Git(..))
    }

    /// Whether this is a local package.
    pub fn is_local(&self) -> bool {
        matches!(self, PackageId::Local(..))
    }

    /// Get the directory name for storing this package.
    pub fn storage_name(&self) -> String {
        match self {
            Self::Registry(registry_id) => registry_id.storage_name(),
            Self::Git(git_id) => git_id.storage_name(),
            Self::Local(local_id) => local_id.storage_name(),
        }
    }
}

impl From<RegistryId> for PackageId {
    fn from(value: RegistryId) -> Self {
        Self::Registry(value)
    }
}

impl From<GitId> for PackageId {
    fn from(value: GitId) -> Self {
        Self::Git(value)
    }
}

impl From<LocalId> for PackageId {
    fn from(value: LocalId) -> Self {
        Self::Local(value)
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
/// An ID of a stored registry package.
pub struct RegistryId {
    id: StrId,
    version: Version,
    url: InternedUrl,
}

impl RegistryId {
    /// Create a new [`RegistryId`].
    pub fn new(id: StrId, version: Version, url: InternedUrl) -> Self {
        Self { id, version, url }
    }

    /// A unique storage type.
    pub const TYPE: &str = "registry";

    /// Get the directory name for storing this package.
    pub fn storage_name(&self) -> String {
        format!(
            "{}-{}-{}-{}",
            Self::TYPE,
            sha256_string(self.url.host_str().unwrap_or_default()),
            self.id,
            self.version
        )
    }

    /// The name of the package.
    pub fn id(&self) -> StrId {
        self.id
    }

    /// Set the name of the package.
    pub fn set_id(&mut self, id: StrId) {
        self.id = id;
    }

    /// Get the version of this package.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Set the version of this package.
    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }

    /// Get the registry url of this package.
    pub fn url(&self) -> InternedUrl {
        self.url
    }
    /// Set the url of this package.
    pub fn set_url(&mut self, url: InternedUrl) {
        self.url = url;
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
/// An ID of a stored git package.
pub struct GitId {
    url: InternedUrl,
    commit: StrId,
}

impl GitId {
    /// Create a new [`GitId`].
    pub fn new(url: InternedUrl, commit: StrId) -> Self {
        Self { url, commit }
    }

    /// A unique storage type.
    pub const TYPE: &str = "git";

    /// Get the directory name for storing this package.
    pub fn storage_name(&self) -> String {
        format!(
            "{}-{}-{}",
            Self::TYPE,
            sha256_string(self.url.as_str()),
            self.commit
        )
    }

    /// Get the repository url of this package.
    pub fn url(&self) -> InternedUrl {
        self.url
    }

    /// Set the url of this package.
    pub fn set_url(&mut self, url: InternedUrl) {
        self.url = url;
    }

    /// Get the checkouted commit of this repository.
    pub fn commit(&self) -> StrId {
        self.commit
    }

    /// Set the checkouted commit of this repository.
    pub fn set_commit(&mut self, commit: StrId) {
        self.commit = commit;
    }
}

#[derive(Deserialize, Debug, Serialize, Clone, Hash, PartialEq, Eq)]
/// An ID of a local package.
pub struct LocalId {
    path: InternedUrl,
}

impl LocalId {
    /// Create a new [`LocalId`].
    pub fn new(path: InternedUrl) -> Self {
        Self { path }
    }

    /// A unique storage type.
    pub const TYPE: &str = "local";

    /// Get the directory name for storing this package.
    pub fn storage_name(&self) -> String {
        format!("{}-{}", Self::TYPE, sha256_string(self.path.as_str()),)
    }

    /// Get the path to the stored package.
    pub fn path(&self) -> InternedUrl {
        self.path
    }

    /// Set the path to the stored package.
    pub fn set_path(&mut self, path: InternedUrl) {
        self.path = path;
    }
}
