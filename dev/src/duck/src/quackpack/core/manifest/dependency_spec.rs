use crate::StrId;
use crate::core::Source;
use crate::core::Version;

#[derive(Debug)]
pub struct DependencySpec {
    manifest_name: StrId,
    versions: Vec<Version>,
    source: Source,
}

impl DependencySpec {
    pub fn new(manifest_name: StrId, versions: Vec<Version>, source: Source) -> Self {
        Self {
            manifest_name,
            versions,
            source,
        }
    }

    pub fn manifest_name(&self) -> StrId {
        self.manifest_name
    }

    pub fn versions(&self) -> &[Version] {
        &self.versions
    }

    pub fn source(&self) -> &Source {
        &self.source
    }
}
