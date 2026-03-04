use std::collections::HashSet;
use std::fmt;

use crate::{
    QuackResult, QuackResultContext as _,
    quackpack::core::{FeatureName, Package},
    util_common::set_once::SetOnce,
};

#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub enum PackageType {
    RootPackage,
    DirectDependency,
    TransientDependency,
}

impl PackageType {
    /// *deepen* `self`, as in „get type for my dependencies”.
    ///
    /// This is mainly used for printing errors, so we can distinguish between transient and direct
    /// dependencies.
    pub fn deepen(&self) -> Self {
        match self {
            Self::RootPackage => Self::DirectDependency,
            Self::DirectDependency => Self::TransientDependency,
            Self::TransientDependency => Self::TransientDependency,
        }
    }
}

impl fmt::Display for PackageType {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let text = match self {
            Self::RootPackage => "root package",
            Self::DirectDependency => "direct dependency",
            Self::TransientDependency => "transient dependency",
        };
        write!(f, "{}", text)
    }
}

#[derive(Debug)]
pub struct CompilerPackage {
    enabled_features: HashSet<FeatureName>,
    package: Package,
    was_compiled: SetOnce,
    pkg_type: PackageType,
}

impl CompilerPackage {
    pub fn new(package: Package, pkg_type: PackageType) -> Self {
        Self {
            enabled_features: HashSet::new(),
            package,
            pkg_type,
            was_compiled: SetOnce::new(),
        }
    }

    pub fn was_compiled(&self) -> bool {
        self.was_compiled.was_set()
    }

    pub fn mark_as_compiled(&mut self) {
        self.was_compiled.set();
    }

    pub fn enabled_features(&self) -> &HashSet<FeatureName> {
        &self.enabled_features
    }

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
                    self.package.manifest().root_description().name()
                )
            })?;
        for feature in features {
            self.enabled_features.insert(feature);
        }
        Ok(())
    }

    pub fn package(&self) -> &Package {
        &self.package
    }

    pub fn package_type(&self) -> PackageType {
        self.pkg_type
    }
}
