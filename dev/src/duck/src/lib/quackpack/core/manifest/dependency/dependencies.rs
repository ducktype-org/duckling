//! Managing all dependencies of the root package.
use std::collections::HashSet;

use super::{DependencyKind, Selector};
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
            let kind = dep.kind();
            let name = dep.name();
            let was_present = !seen_names.insert((kind, name));
            if was_present {
                qp_bail!("multiple {kind} dependencies specify the same name `{name}`",)
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
        self.get_by_selector(Selector::Name(name))
    }

    /// Check if a dependency exists by an alias.
    pub fn has_by_alias(&self, name: StrId) -> bool {
        self.get_by_alias(name).is_some()
    }

    /// Get a dependency by an alias.
    pub fn get_by_alias(&self, name: StrId) -> Option<&Dependency> {
        self.get_by_selector(Selector::Alias(name))
    }

    /// Check if a dependency exists by an effective name.
    pub fn has_by_effective_name(&self, name: StrId) -> bool {
        self.get_by_effective_name(name).is_some()
    }

    /// Get a dependency by a compilation name.
    pub fn get_by_effective_name(&self, name: StrId) -> Option<&Dependency> {
        self.get_by_selector(Selector::EffectiveName(name))
    }

    /// Get a dependency by a given [`Selector`].
    pub fn get_by_selector(&self, selector: Selector) -> Option<&Dependency> {
        self.0.iter().find(|dep| selector.selects(dep))
    }

    /// Get a dependency by a given [`Selector`].
    pub fn has_by_selector(&self, selector: Selector) -> bool {
        self.get_by_selector(selector).is_some()
    }

    /// Filter by [`DependencyKind`].
    pub fn filter_by_kind(&self, kind: DependencyKind) -> impl Iterator<Item = &Dependency> {
        let selector = Selector::Kind(kind);
        self.0.iter().filter(move |dep| selector.selects(dep))
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
