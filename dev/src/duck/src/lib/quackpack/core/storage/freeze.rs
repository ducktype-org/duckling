//! A storage freezefile format.

use serde::{Deserialize, Serialize};

use crate::StrId;
use crate::quackpack::core::full_identity::FullIdentity;
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::{FeatureName, PackageId, Version};

#[derive(Debug, Deserialize, Serialize, Default, Clone, PartialEq, Eq, Hash)]
/// General storage/venv freezefile.
#[serde(rename_all = "kebab-case")]
pub struct VenvFreeze {
    root: RootPackage,
    dependencies: Vec<FreezePackage>,
}

impl VenvFreeze {
    /// Create a new [`VenvFreeze`].
    pub fn new(root: RootPackage, dependencies: Vec<FreezePackage>) -> Self {
        Self { root, dependencies }
    }

    /// Get the root package of this freeze.
    pub fn root(&self) -> &RootPackage {
        &self.root
    }

    /// A mutable counterpart to the [`root`](Self::root).
    pub fn root_mut(&mut self) -> &mut RootPackage {
        &mut self.root
    }

    /// Set the root package of this freeze.
    pub fn set_root(&mut self, root: RootPackage) {
        self.root = root;
    }

    /// Get the dependencies mentioned in this freeze.
    pub fn dependencies(&self) -> &[FreezePackage] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezePackage> {
        &mut self.dependencies
    }

    /// Set the dependencies in this freeze.
    pub fn set_dependencies(&mut self, dependencies: Vec<FreezePackage>) {
        self.dependencies = dependencies;
    }
}

#[derive(Debug, Deserialize, Serialize, Default, Clone, PartialEq, Eq, Hash)]
/// A root package of the freeze.
#[serde(rename_all = "kebab-case")]
pub struct RootPackage {
    name: StrId,
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<Identity>,
}

impl RootPackage {
    /// Create a new [`RootPackage`].
    pub fn new(
        name: StrId,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<Identity>,
    ) -> Self {
        Self {
            name,
            version,
            features,
            dependencies,
        }
    }

    /// Get the name of this package.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Set the name of this package.
    pub fn set_name(&mut self, name: StrId) {
        self.name = name;
    }

    /// Get the version of this package.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Set the version of this package.
    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }

    /// Get all possible features of this package, known at a time of creating this freeze.
    pub fn features(&self) -> &[FeatureName] {
        &self.features
    }

    /// A mutable counterpart to the [`features`](Self::features).
    pub fn features_mut(&mut self) -> &mut Vec<FeatureName> {
        &mut self.features
    }

    /// Set the all known features of this package.
    pub fn set_features(&mut self, features: Vec<FeatureName>) {
        self.features = features;
    }

    /// Get all direct dependencies of this package.
    pub fn dependencies(&self) -> &[Identity] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<Identity> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this package.
    pub fn set_dependencies(&mut self, dependencies: Vec<Identity>) {
        self.dependencies = dependencies;
    }
}

#[derive(Debug, Deserialize, Serialize, Clone, PartialEq, Hash, Eq)]
/// A non-root dependency in a freeze (transitive or direct).
#[serde(rename_all = "kebab-case")]
pub struct FreezePackage {
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<Identity>,
    #[serde(flatten)]
    source: FullIdentity,
}

impl FreezePackage {
    /// Create a new [`FreezePackage`].
    pub fn new(
        source: FullIdentity,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<Identity>,
    ) -> Self {
        Self {
            version,
            features,
            dependencies,
            source,
        }
    }

    /// Cast this dependency to the [`Identity`].
    pub fn as_identity(&self) -> Identity {
        self.source.as_identity()
    }

    /// Get the name of this dependency.
    pub fn name(&self) -> StrId {
        self.source.name()
    }

    /// Get the version of this dependency.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Set the version of this dependency.
    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }

    /// Get all possible features of this dependency.
    pub fn features(&self) -> &[FeatureName] {
        &self.features
    }

    /// A mutable counterpart to the [`features`](Self::features).
    pub fn features_mut(&mut self) -> &mut Vec<FeatureName> {
        &mut self.features
    }

    /// Set all possible features of this dependency.
    pub fn set_features(&mut self, features: Vec<FeatureName>) {
        self.features = features;
    }

    /// Get all direct dependencies of this dependency.
    pub fn dependencies(&self) -> &[Identity] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<Identity> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this dependency.
    pub fn set_dependencies(&mut self, dependencies: Vec<Identity>) {
        self.dependencies = dependencies;
    }

    /// Get the source of this dependency.
    pub fn identity(&self) -> FullIdentity {
        self.source
    }

    /// Set the source of this dependency.
    pub fn set_identity(&mut self, source: FullIdentity) {
        self.source = source;
    }

    /// Cast self to the [`PackageId`].
    pub fn to_package_id(&self) -> PackageId {
        PackageId::new(self.identity(), self.version())
    }
}

impl From<FreezePackage> for PackageId {
    fn from(value: FreezePackage) -> Self {
        value.to_package_id()
    }
}
