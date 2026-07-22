use crate::quackpack::core::Version;
use crate::quackpack::core::full_identity::{FullIdentity, FullKind};
use crate::quackpack::util::interned_url::InternedUrl;
use crate::quackpack::util::with_version::WithVersion;
use crate::util::hash::sha256_string;

impl WithVersion<FullIdentity> {
    /// Get storage name of a package.
    pub fn storage_name(&self) -> String {
        let id = self.value();
        match id.origin().kind() {
            FullKind::Registry => {
                storage_name_for_registry(&id.name(), self.version(), id.origin().url())
            }
            FullKind::Git { commit } => storage_name_for_git(id.origin().url(), &commit),
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
