//! Selectors for easy searching the dependencies vector.

use std::collections::HashSet;

use super::{Dependency, DependencyKind};
use crate::{StrId, quackpack::core::FeatureName};

#[derive(Clone, Eq, PartialEq)]
/// A selector allows to easily filter the dependencies vector.
pub enum Selector<'a> {
    Name(StrId),
    Alias(StrId),
    EffectiveName(StrId),
    EnabledBy(&'a HashSet<FeatureName>),
    Kind(DependencyKind),
    All(Vec<Selector<'a>>),
    Any(Vec<Selector<'a>>),
}

impl<'a> Selector<'a> {
    /// Returns `true` if this [`Selector`] selects a given [`Dependency`].
    pub fn selects(&self, dependency: &Dependency) -> bool {
        match self {
            Self::Name(name) => dependency.name() == *name,
            Self::Alias(alias) => dependency.alias() == Some(*alias),
            Self::EffectiveName(effective_name) => dependency.effective_name() == *effective_name,
            Self::EnabledBy(features) => dependency.is_enabled_for(features),
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
