use crate::StrId;
use crate::quackpack::core::Version;
use crate::quackpack::core::full_identity::{FullIdentity, FullKind, FullOrigin};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::with_version::WithVersion;
use crate::util::hash::sha256_string;

/// Unique identifier of a package, used throughout the whole project.
pub type PackageId = WithVersion<FullIdentity>;

impl PackageId {
    /// Get the [`FullIdentity`] of the package.
    pub fn identity(&self) -> FullIdentity {
        *self.value()
    }

    /// Get the name of the package.
    pub fn name(&self) -> StrId {
        self.identity().name()
    }

    /// Get the [`FullOrigin`] of the package.
    pub fn origin(&self) -> FullOrigin {
        self.identity().origin()
    }

    /// Get the url of the package.
    pub fn url(&self) -> InternedUrl {
        self.origin().url()
    }

    /// Get the package's kind.
    pub fn kind(&self) -> FullKind {
        self.origin().kind()
    }

    /// Get storage name of a package.
    pub fn storage_name(&self) -> String {
        match self.kind() {
            FullKind::Registry => {
                storage_name_for_registry(&self.name(), self.version(), self.url())
            }
            FullKind::Git { commit } => storage_name_for_git(self.url(), &commit),
            FullKind::Local => unreachable!("local packages do not have storage names"),
        }
    }
}

/// Get storage name for git packages.
pub fn storage_name_for_git(url: InternedUrl, commit: &str) -> String {
    format!("git-{}-{}", sha256_string(url.as_str()), commit,)
}

/// Get storage name for repository packages.
pub fn storage_name_for_registry(name: &str, version: Version, url: InternedUrl) -> String {
    format!(
        "registry-{}-{}-{}",
        sha256_string(url.host_str().unwrap_or_default()),
        name,
        version,
    )
}
