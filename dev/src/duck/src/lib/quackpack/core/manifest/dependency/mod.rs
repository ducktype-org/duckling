//! Managing a single dependency abstraction.
use crate::{QuackError, QuackResult, StrId, qp_bail, quackpack::core::FeatureName};

mod conditions;
mod dependency_description;
pub use dependency_description::*;
mod dependencies;
mod dependency_feature;
use crate::quackpack::schemas::registry;
pub use conditions::*;
pub use dependencies::*;
pub use dependency_feature::*;

#[derive(Clone, Debug)]
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
            qp_bail!("only registry sources can be pinned")
        }
        if is_pinned && desc.versions().len() != 1 {
            qp_bail!("pinned dependencies must specify exactly one version")
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

    /// Get the required root packages mentioned in the manifest.
    pub fn enableing_features(&self) -> &[FeatureName] {
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
        self.real_name() != self.desc().manifest_name()
    }

    /// Get an iterator over features that are enabled for the given features.
    pub fn enabled_features<I>(&self, enabled_features: I) -> Vec<FeatureName>
    where
        I: IntoIterator<Item = FeatureName>,
        <I as IntoIterator>::IntoIter: Clone,
    {
        let iter = enabled_features.into_iter();
        self.features
            .iter()
            .filter_map(|feature| {
                if feature.is_enabled_for(iter.clone()) {
                    Some(feature.name())
                } else {
                    None
                }
            })
            .collect()
    }
}

impl TryFrom<(&str, registry::Dependency)> for Dependency {
    type Error = QuackError;

    fn try_from(value: (&str, registry::Dependency)) -> Result<Self, Self::Error> {
        let (real_name, value) = value;
        let real_name = real_name.into();
        let registry::Dependency {
            version,
            source,
            features,
            pinned,
            conditions,
            is_alias_for,
        } = value;
        let manifest_name = is_alias_for.map(Into::into).unwrap_or(real_name);
        let desc = DependencyDescription::new(manifest_name, version, source.try_into()?)?;
        let features = features
            .into_iter()
            .map(TryInto::try_into)
            .collect::<Result<_, _>>()?;
        Self::new(
            desc,
            features,
            pinned,
            Some(conditions.try_into()?),
            real_name,
        )
    }
}

impl TryFrom<Dependency> for registry::Dependency {
    type Error = QuackError;

    fn try_from(value: Dependency) -> Result<Self, Self::Error> {
        let Dependency {
            desc,
            features,
            is_pinned,
            conditions,
            real_name,
        } = value;
        let is_alias_for = if real_name == desc.manifest_name() {
            None
        } else {
            Some(real_name.into())
        };
        let (_, version, source) = desc.decompose();
        let features = features.into_iter().map(Into::into).collect();
        let conditions = match conditions {
            Some(conditions) => conditions.into(),
            None => registry::DependencyCondition {
                package_features: None,
            },
        };
        Ok(Self {
            version,
            source: source.try_into()?,
            features,
            pinned: is_pinned,
            conditions,
            is_alias_for,
        })
    }
}
