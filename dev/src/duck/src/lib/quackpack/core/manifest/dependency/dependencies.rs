//! Managing all dependencies of the root package.
use std::collections::HashSet;

use crate::quackpack::core::Dependency;
use crate::quackpack::schemas::registry;
use crate::{QuackError, QuackResult, StrId, qp_bail};

#[derive(Clone, Debug)]
/// Map of all of the dependencies.
/// Note that it has invariant, that `self.get(name).source().manifest_name() == name`
pub struct Dependencies(Vec<Dependency>);

impl Dependencies {
    /// Create a new [`Dependencies`].
    pub fn new(dependencies: Vec<Dependency>) -> QuackResult<Self> {
        Self::bail_if_has_duplicated_names(&dependencies)?;
        Ok(Self(dependencies))
    }

    /// Bail, if some dependencies have the same name.
    fn bail_if_has_duplicated_names(deps: &[Dependency]) -> QuackResult<()> {
        let mut seen_names = HashSet::new();
        for dep in deps {
            let was_present = !seen_names.insert(dep.name());
            if was_present {
                qp_bail!(
                    "multiple dependencies specify the same name `{}`",
                    dep.name()
                )
            }
        }
        Ok(())
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
        self.0.iter().find(|dep| dep.alias() == Some(name))
    }

    /// Check if a dependency exists by a compilatio name.
    pub fn has_by_effective_name(&self, name: StrId) -> bool {
        self.get_by_effective_name(name).is_some()
    }

    /// Get a dependency by a compilation name.
    pub fn get_by_effective_name(&self, name: StrId) -> Option<&Dependency> {
        self.0.iter().find(|dep| dep.effective_name() == name)
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
        let deps = value
            .into_iter()
            .map(Dependency::try_from)
            .collect::<Result<_, _>>()?;
        Self::new(deps)
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
