use crate::QuackResult;
use crate::StrId;
use crate::qp_bail;
use crate::quackpack::core::InternedSource;
use crate::quackpack::core::Version;

#[derive(Clone, Debug)]
/// Description of a crucial elements of a dependency.
/// Note, that since we allow aliases, `manifest_name` may be an alias specified in the manifest.
/// Real (unaliased) name is in `dependency.real_name`.
pub struct DependencyDescription {
    manifest_name: StrId,
    versions: Vec<Version>,
    source: InternedSource,
}

impl DependencyDescription {
    /// Create a new `DependencyDescription`.
    pub fn new(
        manifest_name: StrId,
        versions: Vec<Version>,
        source: InternedSource,
    ) -> QuackResult<Self> {
        if source.is_registry() && versions.is_empty() {
            qp_bail!("a registry dependency must provide at least one version")
        }
        Ok(Self {
            manifest_name,
            versions,
            source,
        })
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
    pub fn source(&self) -> InternedSource {
        self.source
    }

    /// Destroy this description into inner parts
    pub fn decompose(self) -> (StrId, Vec<Version>, InternedSource) {
        let Self {
            manifest_name,
            versions,
            source,
        } = self;
        (manifest_name, versions, source)
    }
}
