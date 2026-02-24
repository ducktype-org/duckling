use std::{fmt, str::FromStr};

use serde::{Deserialize, Serialize, de, ser};

use crate::{
    QuackError, QuackResultContext, StrId, qp_bail,
    quackpack::core::{
        FeatureName, Version,
        storage::package_id::{GitId, LocalId, PackageId, RegistryId},
        types_common::{ExpandedLocation, InternedExpandedLocation},
    },
};

#[derive(Debug, Deserialize, Serialize, Default, Clone)]
pub struct VenvFreeze {
    root: RootPackage,
    dependencies: Vec<FreezePackage>,
}

impl VenvFreeze {
    pub fn new(root: RootPackage, dependencies: Vec<FreezePackage>) -> Self {
        Self { root, dependencies }
    }

    pub fn root(&self) -> &RootPackage {
        &self.root
    }

    pub fn root_mut(&mut self) -> &mut RootPackage {
        &mut self.root
    }

    pub fn set_root(&mut self, root: RootPackage) {
        self.root = root;
    }

    pub fn dependencies(&self) -> &[FreezePackage] {
        &self.dependencies
    }

    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezePackage> {
        &mut self.dependencies
    }

    pub fn set_dependencies(&mut self, dependencies: Vec<FreezePackage>) {
        self.dependencies = dependencies;
    }
}

#[derive(Debug, Deserialize, Serialize, Default, Clone, PartialEq, Eq)]
pub struct RootPackage {
    name: StrId,
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<FreezeDep>,
}

impl RootPackage {
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

    pub fn name(&self) -> StrId {
        self.name
    }

    pub fn set_name(&mut self, name: StrId) {
        self.name = name;
    }

    pub fn version(&self) -> Version {
        self.version
    }

    pub fn version_mut(&mut self) -> &mut Version {
        &mut self.version
    }

    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }

    pub fn features(&self) -> &[StrId] {
        &self.features
    }

    pub fn features_mut(&mut self) -> &mut Vec<FeatureName> {
        &mut self.features
    }

    pub fn set_features(&mut self, features: Vec<FeatureName>) {
        self.features = features;
    }

    pub fn dependencies(&self) -> &[FreezeDep] {
        &self.dependencies
    }

    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezeDep> {
        &mut self.dependencies
    }

    pub fn set_dependencies(&mut self, dependencies: Vec<FreezeDep>) {
        self.dependencies = dependencies;
    }
}

#[derive(Debug, Deserialize, Serialize, Clone, PartialEq, Eq)]
pub struct FreezePackage {
    name: StrId,
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<FreezeDep>,
    source: InternedExpandedLocation,
}

impl FreezePackage {
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

    pub fn name(&self) -> StrId {
        self.name
    }

    pub fn set_name(&mut self, name: StrId) {
        self.name = name;
    }

    pub fn version(&self) -> Version {
        self.version
    }

    pub fn version_mut(&mut self) -> &mut Version {
        &mut self.version
    }

    pub fn set_version(&mut self, version: Version) {
        self.version = version;
    }

    pub fn features(&self) -> &[StrId] {
        &self.features
    }

    pub fn features_mut(&mut self) -> &mut Vec<FeatureName> {
        &mut self.features
    }

    pub fn set_features(&mut self, features: Vec<FeatureName>) {
        self.features = features;
    }

    pub fn dependencies(&self) -> &[FreezeDep] {
        &self.dependencies
    }

    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezeDep> {
        &mut self.dependencies
    }

    pub fn set_dependencies(&mut self, dependencies: Vec<FreezeDep>) {
        self.dependencies = dependencies;
    }

    pub fn source(&self) -> InternedExpandedLocation {
        self.source
    }

    pub fn set_source(&mut self, source: InternedExpandedLocation) {
        self.source = source;
    }

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

#[derive(Debug, Default, Copy, Clone, Hash, PartialEq, Eq)]
/// Dependency deserialized from format `<name> <version>`
pub struct FreezeDep {
    name: StrId,
    version: Version,
}

impl FreezeDep {
    pub fn new(name: StrId, version: Version) -> Self {
        Self { name, version }
    }

    pub fn name(&self) -> StrId {
        self.name
    }

    pub fn set_name(&mut self, name: StrId) {
        self.name = name;
    }

    pub fn version(&self) -> Version {
        self.version
    }

    pub fn version_mut(&mut self) -> &mut Version {
        &mut self.version
    }

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
