use std::collections::HashMap;

use crate::{StrId, quackpack::core::Dependency};

#[derive(Debug)]
/// Map of all of the dependencies.
/// Note that it has invariant, that `self.get(name).source().manifest_name() == name`
pub struct Dependencies(HashMap<StrId, Dependency>);

impl Dependencies {
    /// Create a new dependencies map.
    pub fn new(dependencies: HashMap<StrId, Dependency>) -> Self {
        Self(dependencies)
    }

    /// Check if a dependency exists by name.
    pub fn has_dependency(&self, name: StrId) -> bool {
        self.get_dependency(name).is_some()
    }

    /// Get a dependency by name.
    pub fn get_dependency(&self, name: StrId) -> Option<&Dependency> {
        self.0.get(&name)
    }

    /// Get an iterator over all dependencies.
    pub fn all_dependencies(&self) -> &HashMap<StrId, Dependency> {
        &self.0
    }
}
