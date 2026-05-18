//! Required informations (+ informations for better errors) for compiling a single package.
//!
//! In general, this is [`Package`] + enabled features.
use std::collections::HashSet;
use std::fmt;

use crate::quackpack::core::{FeatureName, Package};
use crate::{QuackResult, QuackResultContext};

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
/// Describes what type dependency type, in respect to the root, is this package.
pub enum PackageType {
    RootPackage,
    DirectDependency,
    TransitiveDependency,
}

impl PackageType {
    /// *deepen* `self`, as in „get type for my dependencies”.
    ///
    /// This is mainly used for printing errors, so we can distinguish between transitive and direct
    /// dependencies.
    pub fn deepen(&self) -> Self {
        match self {
            Self::RootPackage => Self::DirectDependency,
            Self::DirectDependency => Self::TransitiveDependency,
            Self::TransitiveDependency => Self::TransitiveDependency,
        }
    }
}

impl fmt::Display for PackageType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let text = match self {
            Self::RootPackage => "root package",
            Self::DirectDependency => "direct dependency",
            Self::TransitiveDependency => "transitive dependency",
        };
        write!(f, "{}", text)
    }
}

#[derive(Debug)]
/// An abstraction over complete information required to compile a single package.
pub struct CompilerPackage {
    enabled_features: HashSet<FeatureName>,
    package: Package,
    pkg_type: PackageType,
}

impl CompilerPackage {
    /// Create a new [`CompilerPackage`], with empty features.
    pub fn new(package: Package, pkg_type: PackageType) -> Self {
        Self {
            enabled_features: HashSet::new(),
            package,
            pkg_type,
        }
    }

    /// Get all currently enabled features for this package.
    pub fn enabled_features(&self) -> &HashSet<FeatureName> {
        &self.enabled_features
    }

    /// Add new features as enabled for this package.
    ///
    /// Note, that:
    /// 1. new features are recursively expanded,
    /// 2. there are no duplicates,
    /// 3. there are no guarantees about order,
    /// 4. if there's a nonexistent feature requested, this method will fail.
    pub fn add_new_features(
        &mut self,
        features: impl IntoIterator<Item = FeatureName>,
    ) -> QuackResult<()> {
        let features = self
            .package
            .manifest()
            .features()
            .expand_features(features)
            .with_context(|| {
                format!(
                    "while expanding features of the {} `{}`",
                    self.pkg_type,
                    self.package.manifest().name()
                )
            })?;
        for feature in features {
            self.enabled_features.insert(feature);
        }
        Ok(())
    }

    /// As [`CompilerPackage::add_new_features`], but returns the set of features that would be added.
    pub fn mock_add_features(
        &self,
        features: impl IntoIterator<Item = FeatureName>,
    ) -> QuackResult<HashSet<FeatureName>> {
        let features = self
            .package
            .manifest()
            .features()
            .expand_features(features)
            .with_context(|| {
                format!(
                    "while expanding features of the {} `{}`",
                    self.pkg_type,
                    self.package.manifest().name()
                )
            })?;
        Ok(features)
    }

    /// Get the underlying [`Package`].
    pub fn package(&self) -> &Package {
        &self.package
    }

    /// Get the [`PackageType`] of this package.
    pub fn package_type(&self) -> PackageType {
        self.pkg_type
    }
}
