// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Managing a single dependency abstraction.
use std::collections::HashSet;
use std::fmt;

use crate::quackpack::core::valid_package_name::{normalise_package_name, validate_package_name};
use crate::quackpack::core::{FeatureName, Source, Version};
use crate::{QuackError, QuackResult, QuackResultContext, StrId, qp_bail};

mod conditions;
mod dependencies;
mod dependency_feature;
mod selector;
pub use conditions::*;
pub use dependencies::*;
pub use dependency_feature::*;
pub use selector::*;

use crate::quackpack::schemas::registry;

#[derive(Debug, Clone, Copy, Eq, PartialEq, Ord, PartialOrd, Hash)]
/// A [`Dependency`] kind.
pub enum DependencyKind {
    /// A normal dependency, comes from `dependencies:` map.
    Normal,
    /// A dev dependency, comes from `dev-dependencies:` map.
    Dev,
}

impl DependencyKind {
    /// Returns `true` if the dependency kind is [`Normal`].
    ///
    /// [`Normal`]: DependencyKind::Normal
    #[must_use]
    pub fn is_normal(self) -> bool {
        matches!(self, Self::Normal)
    }

    /// Returns `true` if the dependency kind is [`Dev`].
    ///
    /// [`Dev`]: DependencyKind::Dev
    #[must_use]
    pub fn is_dev(self) -> bool {
        matches!(self, Self::Dev)
    }

    pub fn key_in_manifest(self) -> &'static str {
        match self {
            DependencyKind::Normal => "dependencies",
            DependencyKind::Dev => "dev-dependencies",
        }
    }
}

impl fmt::Display for DependencyKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Normal => write!(f, "normal"),
            Self::Dev => write!(f, "dev"),
        }
    }
}

#[derive(Clone, Debug)]
/// High level abstraction on a package's dependency.
pub struct Dependency {
    /// Features of this dependency.
    features: Vec<DependencyFeature>,
    /// Whether this dependency is pinned to a specific version.
    is_pinned: bool,
    /// Optional conditions for this dependency to be enabled.
    conditions: Option<Conditions>,
    /// The real (unaliased) name of the dependency.
    name: StrId,
    /// Alias specified in the manifest.
    alias: Option<StrId>,
    /// All versions of this dependency.
    versions: Vec<Version>,
    /// Source of this dependency.
    source: Source,
    /// Type of this dependency.
    kind: DependencyKind,
}

impl Dependency {
    /// Create a new [`Dependency`].
    ///
    /// This function will fail if and only if:
    /// 1. `is_pinned` is true and `desc` doesn't have a [`Registry`](super::Registry) source.
    /// 2. `is_pinned` is true and `desc.versions()` doesn't have a length 1.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        name: StrId,
        versions: Vec<Version>,
        source: Source,
        features: Vec<DependencyFeature>,
        is_pinned: bool,
        conditions: Option<Conditions>,
        alias: Option<StrId>,
        kind: DependencyKind,
    ) -> QuackResult<Self> {
        debug_assert_ne!(
            Some(name),
            alias,
            "alias should be None, if it's the same as name"
        );
        if source.is_registry() && versions.is_empty() {
            qp_bail!("a registry dependency must provide at least one version")
        }
        if is_pinned && !source.is_registry() {
            qp_bail!("only registry sources can be pinned")
        }
        if is_pinned && versions.len() != 1 {
            qp_bail!("pinned dependencies must specify exactly one version")
        }
        Ok(Self {
            features,
            is_pinned,
            conditions,
            name,
            alias,
            versions,
            source,
            kind,
        })
    }

    /// Get the real (unaliased) name of the dependency.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Get the list of dependency features.
    pub fn features(&self) -> &[DependencyFeature] {
        &self.features
    }

    /// Check if this dependency is pinned to a specific version.
    pub fn is_pinned(&self) -> bool {
        self.is_pinned
    }

    /// Check if this dependency is enabled for the given features.
    pub fn is_enabled_for(&self, enabled_features: &HashSet<FeatureName>) -> bool {
        self.conditions
            .as_ref()
            .is_none_or(|conditions| conditions.is_enabled_for(enabled_features))
    }

    /// Get the required root packages mentioned in the manifest.
    pub fn enabling_features(&self) -> &[FeatureName] {
        let Some(conditions) = &self.conditions else {
            return &[];
        };
        let Some(features) = conditions.required_root_package_features() else {
            return &[];
        };
        features
    }

    /// Whether this dependency was aliased in the manifest.
    pub fn is_aliased(&self) -> bool {
        self.alias.is_some()
    }

    /// Get an iterator over features that are enabled for the given features.
    pub fn enabled_features(&self, enabled_features: &HashSet<FeatureName>) -> Vec<FeatureName> {
        self.features
            .iter()
            .filter_map(|feature| {
                if feature.is_enabled_for(enabled_features) {
                    Some(feature.name())
                } else {
                    None
                }
            })
            .collect()
    }

    /// Get the aliased name of this package.
    /// If none, then this package has not been aliased.
    pub fn alias(&self) -> Option<StrId> {
        self.alias
    }

    /// Get the normalised aliased name of this package.
    /// If none, then this package has not been aliased.
    pub fn normalised_alias(&self) -> Option<String> {
        self.alias.map(|alias| normalise_package_name(&alias))
    }

    /// Get versions of this package.
    pub fn versions(&self) -> &[Version] {
        &self.versions
    }

    /// Get the source of this package.
    pub fn source(&self) -> Source {
        self.source
    }

    /// Get the effective name of this dependency.
    ///
    /// Helper for `self.alias().unwrap_or(self.name())`.
    pub fn effective_name(&self) -> StrId {
        self.alias.unwrap_or(self.name)
    }

    /// Get [`DependencyKind`] of this dependency.
    pub fn kind(&self) -> DependencyKind {
        self.kind
    }

    /// Get the [`Conditions`] of this dependency.
    pub fn conditions(&self) -> Option<&Conditions> {
        self.conditions.as_ref()
    }
}

impl TryFrom<registry::Dependency> for Dependency {
    type Error = QuackError;

    fn try_from(value: registry::Dependency) -> Result<Self, Self::Error> {
        let registry::Dependency {
            name,
            version,
            source,
            features,
            pinned,
            conditions,
            alias,
            kind,
        } = value;
        validate_package_name(&name)
            .context("registry responded with a dependency with an invalid name")?;

        if let Some(ref alias) = alias {
            validate_package_name(alias)
                .context("registry responded with a dependency with an invalid alias")?;
        }

        let alias = alias.map(StrId::from);
        let name = name.into();
        if alias == Some(name) {
            qp_bail!("registry dependency `{name}` specified itself as an alias")
        }
        let features = features
            .into_iter()
            .map(TryInto::try_into)
            .collect::<Result<_, _>>()?;
        Self::new(
            name,
            version,
            source.try_into()?,
            features,
            pinned,
            Some(conditions.into()),
            alias,
            kind.into(),
        )
    }
}

impl TryFrom<Dependency> for registry::Dependency {
    type Error = QuackError;

    fn try_from(value: Dependency) -> Result<Self, Self::Error> {
        let Dependency {
            features,
            is_pinned,
            conditions,
            name,
            alias,
            versions,
            source,
            kind,
        } = value;
        let features = features.into_iter().map(Into::into).collect();
        let conditions = match conditions {
            Some(conditions) => conditions.into(),
            None => registry::DependencyCondition {
                package_features: None,
            },
        };
        Ok(Self {
            version: versions,
            source: source.try_into()?,
            features,
            pinned: is_pinned,
            conditions,
            alias: alias.map(Into::into),
            name: name.into(),
            kind: kind.into(),
        })
    }
}

impl From<registry::DependencyKind> for DependencyKind {
    fn from(value: registry::DependencyKind) -> Self {
        match value {
            registry::DependencyKind::Normal => Self::Normal,
            registry::DependencyKind::Dev => Self::Dev,
        }
    }
}

impl From<DependencyKind> for registry::DependencyKind {
    fn from(value: DependencyKind) -> Self {
        match value {
            DependencyKind::Normal => Self::Normal,
            DependencyKind::Dev => Self::Dev,
        }
    }
}
