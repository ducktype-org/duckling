//! Selectors for easy searching the dependencies vector.

use super::{Dependency, DependencyKind};
use crate::StrId;

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
/// A selector allows to easily filter the dependencies vector.
pub enum Selector {
    Name(StrId),
    Alias(StrId),
    EffectiveName(StrId),
    Kind(DependencyKind),
    All(Vec<Selector>),
    Any(Vec<Selector>),
}

impl Selector {
    /// Returns `true` if this [`Selector`] selects a given [`Dependency`].
    pub fn selects(&self, dependency: &Dependency) -> bool {
        match self {
            Self::Name(name) => dependency.name() == *name,
            Self::Alias(alias) => dependency.alias() == Some(*alias),
            Self::EffectiveName(effective_name) => dependency.effective_name() == *effective_name,
            Self::Kind(kind) => dependency.kind() == *kind,
            Self::All(selectors) => selectors
                .iter()
                .all(|selector| selector.selects(dependency)),
            Self::Any(selectors) => selectors
                .iter()
                .any(|selector| selector.selects(dependency)),
        }
    }
}
