use std::collections::HashMap;

use crate::{StrId, core::Dependency};

#[derive(Debug)]
pub struct Dependencies(HashMap<StrId, Dependency>);

impl Dependencies {
    pub fn new(dependencies: HashMap<StrId, Dependency>) -> Self {
        Self(dependencies)
    }

    pub fn has_dependency(&self, name: StrId) -> bool {
        self.get_dependency(name).is_some()
    }

    pub fn get_dependency(&self, name: StrId) -> Option<&Dependency> {
        self.0.get(&name)
    }

    pub fn all_dependencies(&self) -> &HashMap<StrId, Dependency> {
        &self.0
    }
}
