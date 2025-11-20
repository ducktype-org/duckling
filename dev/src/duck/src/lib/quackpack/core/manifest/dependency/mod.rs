use crate::{QuackResult, StrId, quackpack::core::FeatureName};

mod conditions;
mod dependency_description;
pub use dependency_description::*;
mod dependencies;
mod dependency_feature;
use anyhow::bail;
pub use conditions::*;
pub use dependencies::*;
pub use dependency_feature::*;

#[derive(Debug)]
/// High level abstraction on a package's dependency.
pub struct Dependency {
    /// The dependency description.
    desc: DependencyDescription,
    /// Features of this dependency.
    features: Vec<DependencyFeature>,
    /// Whether this dependency is pinned to a specific version.
    is_pinned: bool,
    /// Optional conditions for this dependency to be enabled.
    conditions: Option<Conditions>,
    /// The real (unaliased) name of the dependency.
    real_name: StrId,
}

impl Dependency {
    /// Create a new [`Dependency`].
    ///
    /// This function will fail if and only if:
    /// 1. `is_pinned` is true and `desc` doesn't have a [`Registry`](super::Registry) source.
    /// 2. `is_pinned` is true and `desc.versions()` doesn't have a length 1.
    pub fn new(
        desc: DependencyDescription,
        features: Vec<DependencyFeature>,
        is_pinned: bool,
        conditions: Option<Conditions>,
        real_name: StrId,
    ) -> QuackResult<Self> {
        if is_pinned && !desc.source().is_registry() {
            bail!("only registry sources can be pinned")
        }
        if is_pinned && desc.versions().len() != 1 {
            bail!("pinned dependencies must specify exactly one version")
        }
        Ok(Self {
            desc,
            features,
            is_pinned,
            conditions,
            real_name,
        })
    }

    /// Get the dependency description.
    pub fn desc(&self) -> &DependencyDescription {
        &self.desc
    }

    /// Get the real (unaliased) name of the dependency.
    pub fn real_name(&self) -> StrId {
        self.real_name
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
    pub fn is_enabled_for(&self, enabled_features: impl IntoIterator<Item = FeatureName>) -> bool {
        self.conditions
            .as_ref()
            .is_none_or(|conditions| conditions.is_enabled_for(enabled_features))
    }

    /// Get an iterator over features that are enabled for the given features.
    // NOTE: We take `Vec`, because it has trivially a copyable iterator (iterator over a slice).
    pub fn enabled_features(&self, enabled_features: Vec<FeatureName>) -> Vec<FeatureName> {
        self.features
            .iter()
            .filter_map(|feature| {
                if feature.is_enabled_for(enabled_features.iter().copied()) {
                    Some(feature.name())
                } else {
                    None
                }
            })
            .collect()
    }
}
