//! A storage freezefile format.

use serde::{Deserialize, Serialize};

use crate::StrId;
use crate::quackpack::core::identity::{Identity, Kind};
use crate::quackpack::core::simple_identity::SimpleIdentity;
use crate::quackpack::core::storage::package_id::{GitId, LocalId, PackageId, RegistryId};
use crate::quackpack::core::{FeatureName, Version};

#[derive(Debug, Deserialize, Serialize, Default, Clone, PartialEq, Eq, Hash)]
/// General storage/venv freezefile.
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
pub struct RootPackage {
    name: StrId,
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<SimpleIdentity>,
}

impl RootPackage {
    /// Create a new [`RootPackage`].
    pub fn new(
        name: StrId,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<SimpleIdentity>,
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
    pub fn dependencies(&self) -> &[SimpleIdentity] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<SimpleIdentity> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this package.
    pub fn set_dependencies(&mut self, dependencies: Vec<SimpleIdentity>) {
        self.dependencies = dependencies;
    }
}

#[derive(Debug, Deserialize, Serialize, Clone, PartialEq, Hash, Eq)]
/// A non-root dependency in a freeze (transitive or direct).
pub struct FreezePackage {
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<SimpleIdentity>,
    #[serde(flatten)]
    source: Identity,
}

impl FreezePackage {
    /// Create a new [`FreezePackage`].
    pub fn new(
        source: Identity,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<SimpleIdentity>,
    ) -> Self {
        Self {
            version,
            features,
            dependencies,
            source,
        }
    }

    /// Cast this dependency to the [`SimpleIdentity`].
    pub fn as_simple_identity(&self) -> SimpleIdentity {
        self.source.as_simple()
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
    pub fn dependencies(&self) -> &[SimpleIdentity] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<SimpleIdentity> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this dependency.
    pub fn set_dependencies(&mut self, dependencies: Vec<SimpleIdentity>) {
        self.dependencies = dependencies;
    }

    /// Get the source of this dependency.
    pub fn identity(&self) -> &Identity {
        &self.source
    }

    /// Set the source of this dependency.
    pub fn set_identity(&mut self, source: Identity) {
        self.source = source;
    }

    /// Cast self to the [`PackageId`].
    pub fn to_package_id(&self) -> PackageId {
        let url = self.identity().origin().url();
        match self.identity().origin().kind() {
            Kind::Registry => {
                PackageId::Registry(RegistryId::new(self.name(), self.version(), url))
            }
            Kind::Git { commit } => PackageId::Git(GitId::new(url, commit)),
            Kind::Local => PackageId::Local(LocalId::new(url)),
        }
    }
}

impl From<FreezePackage> for PackageId {
    fn from(value: FreezePackage) -> Self {
        value.to_package_id()
    }
}
