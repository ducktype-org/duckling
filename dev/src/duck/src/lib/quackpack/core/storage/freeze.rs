//! A storage freezefile format.
use std::{fmt, str::FromStr};

use serde::{Deserialize, Serialize, de, ser};

use crate::{
    QuackError, QuackResultContext, StrId, qp_bail,
    quackpack::core::{
        FeatureName, Version,
        solver::types_common::{ExpandedLocation, InternedExpandedLocation},
        storage::package_id::{GitId, LocalId, PackageId, RegistryId},
    },
};

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
    dependencies: Vec<FreezeDep>,
}

impl RootPackage {
    /// Create a new [`RootPackage`].
    pub fn new(
        name: StrId,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<FreezeDep>,
    ) -> Self {
        Self {
            name,
            version,
            features,
            dependencies,
        }
    }

    /// Cast this root package to the [`FreezeDep`].
    pub fn as_freeze_dep(&self) -> FreezeDep {
        FreezeDep {
            name: self.name(),
            version: self.version(),
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
    pub fn dependencies(&self) -> &[FreezeDep] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezeDep> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this package.
    pub fn set_dependencies(&mut self, dependencies: Vec<FreezeDep>) {
        self.dependencies = dependencies;
    }
}

#[derive(Debug, Deserialize, Serialize, Clone, PartialEq, Hash, Eq)]
/// A non-root dependency in a freeze (transitive or direct).
pub struct FreezePackage {
    name: StrId,
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<FreezeDep>,
    source: InternedExpandedLocation,
}

impl FreezePackage {
    /// Create a new [`FreezePackage`].
    pub fn new(
        name: StrId,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<FreezeDep>,
        source: InternedExpandedLocation,
    ) -> Self {
        Self {
            name,
            version,
            features,
            dependencies,
            source,
        }
    }

    /// Cast this dependency to the [`FreezeDep`].
    pub fn as_freeze_dep(&self) -> FreezeDep {
        FreezeDep {
            name: self.name(),
            version: self.version(),
        }
    }

    /// Get the name of this dependency.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Set the name of this dependency.
    pub fn set_name(&mut self, name: StrId) {
        self.name = name;
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
    pub fn dependencies(&self) -> &[FreezeDep] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezeDep> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this dependency.
    pub fn set_dependencies(&mut self, dependencies: Vec<FreezeDep>) {
        self.dependencies = dependencies;
    }

    /// Get the source of this dependency.
    pub fn source(&self) -> InternedExpandedLocation {
        self.source
    }

    /// Set the source of this dependency.
    pub fn set_source(&mut self, source: InternedExpandedLocation) {
        self.source = source;
    }

    /// Cast self to the [`PackageId`].
    pub fn to_package_id(&self) -> PackageId {
        match self.source().as_ref() {
            ExpandedLocation::Registry { url, .. } => {
                PackageId::Registry(RegistryId::new(self.name(), self.version(), url.clone()))
            }
            ExpandedLocation::Git { url, commit } => {
                PackageId::Git(GitId::new(url.clone(), *commit))
            }
            ExpandedLocation::Local { absolute_path } => {
                PackageId::Local(LocalId::new(absolute_path.clone()))
            }
        }
    }
}

impl From<FreezePackage> for PackageId {
    fn from(value: FreezePackage) -> Self {
        value.to_package_id()
    }
}

#[derive(Debug, Default, Clone, Copy, Hash, Eq, PartialEq)]
/// Dependency deserialized from format `<name> <version>`
/// It's used as a values in dependencies's of a node.
pub struct FreezeDep {
    name: StrId,
    version: Version,
}

impl FreezeDep {
    /// Create a new [`FreezeDep`].
    pub fn new(name: StrId, version: Version) -> Self {
        Self { name, version }
    }

    /// Get the name of this dependency.
    pub fn name(&self) -> StrId {
        self.name
    }

    /// Set the name of this dependency.
    pub fn set_name(&mut self, name: StrId) {
        self.name = name;
    }

    /// Get the version of this dependency.
    pub fn version(&self) -> Version {
        self.version
    }

    /// Set the version of this dependency.
    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }
}

impl fmt::Display for FreezeDep {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} {}", self.name(), self.version())
    }
}

impl FromStr for FreezeDep {
    type Err = QuackError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let mut iter = s.split(' ');
        let (name, version) = match (iter.next(), iter.next(), iter.next()) {
            (Some(name), Some(version), None) => (name, version),
            _ => qp_bail!("expected a freeze dependency in format `<name> <version>`"),
        };
        let version = version
            .parse()
            .with_context(|| format!("freeze dependency `{}` has invalid version syntax", s))?;
        Ok(Self::new(name.into(), version))
    }
}

impl ser::Serialize for FreezeDep {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        serializer.collect_str(self)
    }
}

impl<'de> de::Deserialize<'de> for FreezeDep {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        let string = <&'de str>::deserialize(deserializer)?;
        Self::from_str(string).map_err(de::Error::custom)
    }
}
