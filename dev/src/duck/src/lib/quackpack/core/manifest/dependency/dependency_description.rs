use crate::StrId;
use crate::quackpack::core::Source;
use crate::quackpack::core::Version;

#[derive(Debug)]
/// Description of a crucial elements of a dependency.
/// Note, that since we allow aliases, `manifest_name` may be an alias specified in the manifest.
/// Real (unaliased) name is in `dependency.real_name`.
pub struct DependencyDescription {
    manifest_name: StrId,
    versions: Vec<Version>,
    source: Source,
}

impl DependencyDescription {
    /// Create a new `DependencyDescription`.
    pub fn new(manifest_name: StrId, versions: Vec<Version>, source: Source) -> Self {
        Self {
            manifest_name,
            versions,
            source,
        }
    }

    /// Get the name of the dependency, specified in the manifest (may be an alias).
    pub fn manifest_name(&self) -> StrId {
        self.manifest_name
    }

    /// Get versions required by a dependency.
    pub fn versions(&self) -> &[Version] {
        &self.versions
    }

    /// Get the dependency source.
    pub fn source(&self) -> &Source {
        &self.source
    }
}
