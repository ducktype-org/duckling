use crate::{StrId, core::Version};
#[derive(Debug)]
pub struct RootSpec {
    name: StrId,
    version: Version,
}

impl RootSpec {
    pub fn new(name: StrId, version: Version) -> Self {
        Self { name, version }
    }

    pub fn name(&self) -> StrId {
        self.name
    }

    pub fn version(&self) -> Version {
        self.version
    }
}
