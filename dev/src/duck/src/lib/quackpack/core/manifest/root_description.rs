use crate::{StrId, quackpack::core::Version};

#[derive(Debug)]
/// Crucial description of a root package we are working on.
pub struct RootDescription {
    name: StrId,
    version: Version,
}

impl RootDescription {
    /// Create a new root package description.
    pub fn new(name: StrId, version: Version) -> Self {
        Self { name, version }
    }

    /// Get the package name.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Get the package version.
    pub fn version(&self) -> Version {
        self.version
    }
}
