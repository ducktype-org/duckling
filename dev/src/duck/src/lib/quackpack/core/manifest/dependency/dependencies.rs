use std::collections::HashMap;

use crate::QuackError;
use crate::quackpack::schemas::registry;
use crate::{StrId, quackpack::core::Dependency};

#[derive(Clone, Debug)]
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

impl TryFrom<registry::Dependencies> for Dependencies {
    type Error = QuackError;

    fn try_from(value: registry::Dependencies) -> Result<Self, Self::Error> {
        value
            .into_iter()
            .map(|(name, dep)| {
                let dep = Dependency::try_from((name.as_str(), dep))?;
                Ok((name.into(), dep))
            })
            .collect::<Result<_, _>>()
            .map(Self)
    }
}

impl TryFrom<Dependencies> for registry::Dependencies {
    type Error = QuackError;

    fn try_from(value: Dependencies) -> Result<Self, Self::Error> {
        value
            .0
            .into_iter()
            .map(|(name, dep)| Ok((name.into(), dep.try_into()?)))
            .collect::<Result<_, _>>()
    }
}
