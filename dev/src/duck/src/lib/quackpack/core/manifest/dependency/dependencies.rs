//! Managing all dependencies of the root package.
use crate::QuackError;
use crate::quackpack::schemas::registry;
use crate::{StrId, quackpack::core::Dependency};

#[derive(Clone, Debug)]
/// Map of all of the dependencies.
/// Note that it has invariant, that `self.get(name).source().manifest_name() == name`
pub struct Dependencies(Vec<Dependency>);

impl Dependencies {
    /// Create a new [`Dependencies`].
    pub fn new(dependencies: Vec<Dependency>) -> Self {
        Self(dependencies)
    }

    /// Check if a dependency exists by a name.
    pub fn has_by_name(&self, name: StrId) -> bool {
        self.get_by_name(name).is_some()
    }

    /// Get a dependency by a name.
    pub fn get_by_name(&self, name: StrId) -> Option<&Dependency> {
        self.0.iter().find(|dep| dep.name() == name)
    }

    /// Check if a dependency exists by an alias.
    pub fn has_by_alias(&self, name: StrId) -> bool {
        self.get_by_alias(name).is_some()
    }

    /// Get a dependency by an alias.
    pub fn get_by_alias(&self, name: StrId) -> Option<&Dependency> {
        self.0
            .iter()
            .find(|dep| dep.explicit_manifest_name() == Some(name))
    }

    /// Check if a dependency exists by a compilatio name.
    pub fn has_by_compilation_name(&self, name: StrId) -> bool {
        self.get_by_compilation_name(name).is_some()
    }

    /// Get a dependency by a compilation name.
    pub fn get_by_compilation_name(&self, name: StrId) -> Option<&Dependency> {
        self.0.iter().find(|dep| dep.name_for_compilation() == name)
    }

    /// Get an iterator over all dependencies.
    pub fn all_dependencies(&self) -> &[Dependency] {
        &self.0
    }

    /// Get a mutable iterator over all dependencies.
    pub fn all_dependencies_mut(&mut self) -> &mut Vec<Dependency> {
        &mut self.0
    }
}

impl TryFrom<registry::Dependencies> for Dependencies {
    type Error = QuackError;

    fn try_from(value: registry::Dependencies) -> Result<Self, Self::Error> {
        value
            .into_iter()
            .map(Dependency::try_from)
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
            .map(TryInto::try_into)
            .collect::<Result<_, _>>()
    }
}
