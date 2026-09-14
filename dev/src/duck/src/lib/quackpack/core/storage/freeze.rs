//! A storage freezefile format.

use std::fmt::Display;
use std::str::FromStr;

use serde::{Deserialize, Serialize, de};

use crate::{QuackError, QuackResultContext, StrId, qp_bail};
use crate::quackpack::core::full_identity::FullIdentity;
use crate::quackpack::core::identity::{Identity, Origin};
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
    dependencies: Vec<DepIdWithAlias>,
}

impl RootPackage {
    /// Create a new [`RootPackage`].
    pub fn new(
        name: StrId,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<DepIdWithAlias>,
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
    pub fn dependencies(&self) -> &[DepIdWithAlias] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<DepIdWithAlias> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this package.
    pub fn set_dependencies(&mut self, dependencies: Vec<DepIdWithAlias>) {
        self.dependencies = dependencies;
    }
}

#[derive(Debug, Deserialize, Serialize, Clone, PartialEq, Hash, Eq)]
/// A non-root dependency in a freeze (transitive or direct).
#[serde(rename_all = "kebab-case")]
pub struct FreezePackage {
    #[serde(flatten)]
    source: FullIdentity,
    version: Version,
    features: Vec<FeatureName>,
    dependencies: Vec<DepIdWithAlias>,
}

impl FreezePackage {
    /// Create a new [`FreezePackage`].
    pub fn new(
        source: FullIdentity,
        version: Version,
        features: Vec<FeatureName>,
        dependencies: Vec<DepIdWithAlias>,
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
    pub fn dependencies(&self) -> &[DepIdWithAlias] {
        &self.dependencies
    }

    /// A mutable counterpart to the [`dependencies`](Self::dependencies).
    pub fn dependencies_mut(&mut self) -> &mut Vec<DepIdWithAlias> {
        &mut self.dependencies
    }

    /// Set the direct dependencies of this dependency.
    pub fn set_dependencies(&mut self, dependencies: Vec<DepIdWithAlias>) {
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

#[derive(Debug, Clone, Copy, Eq, Hash)]
/// How dependencies are described in freeze.
pub struct DepIdWithAlias {
    /// Whether and how the dependency is aliased.
    alias: Option<StrId>,
    /// [`Identity`] of the dependency.
    id: Identity,
}

impl DepIdWithAlias {
    /// Create new [`DepIdWithAlias`].
    pub fn new(effective_name: StrId, identity: Identity) -> Self {
        if effective_name != identity.name() {
            Self {
                alias: Some(effective_name),
                id: identity
            }
        } else {
            Self {
                alias: None,
                id: identity
            }
        }
    }

    /// Alias if there is any, otherwise name.
    pub fn effective_name(self) -> StrId {
        if let Some(alias) = self.alias {
            alias
        } else {
            self.id.name()
        }
    }

    /// Get the underlying [`Identity`].
    pub fn identity(self) -> Identity {
        self.id
    }

    /// Get the true (unaliased) name.
    pub fn name(self) -> StrId {
        self.id.name()
    }

    /// Get the underlying [`Origin`].
    pub fn origin(self) -> Origin {
        self.id.origin()
    }
}

impl From<DepIdWithAlias> for Identity {
    fn from(value: DepIdWithAlias) -> Self {
        value.identity()
    }
}

impl Display for DepIdWithAlias {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        if let Some(alias) = self.alias {
            write!(f, "{}#{alias} {}", self.name(), self.origin())
        } else {
            write!(f, "{} {}", self.name(), self.origin())
        }
    }
}

impl PartialEq<DepIdWithAlias> for DepIdWithAlias {
    fn eq(&self, other: &DepIdWithAlias) -> bool {
        self.name() == other.name() && self.origin() == other.origin() && self.alias == other.alias
    }
}

// DepIdWithAlias serialiazes as:
// * `<name>#<alias> <origin>` if there is an alias,
// * `<name> <origin>` if there is no alias.
impl FromStr for DepIdWithAlias {
    type Err = QuackError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let Some((name, origin)) = s.split_once(' ') else {
            qp_bail!("expected a simple identity with alias in format `<name>(#<maybe alias>) <origin>`")
        };
        let origin = origin
            .parse()
            .with_context(|| format!("simple identity `{}` has invalid origin syntax", s))?;
        if let Some((name, alias)) = name.split_once("#") {
            Ok(Self::new(alias.into(), Identity::new(name.into(), origin)))
        } else {
            Ok(Self::new(name.into(), Identity::new(name.into(), origin)))
        }
    }
}

impl Serialize for DepIdWithAlias {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.collect_str(self)
    }
}

impl<'de> Deserialize<'de> for DepIdWithAlias {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a simple identity")
            .string(|s| s.parse().map_err(de::Error::custom))
            .deserialize(deserializer)
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use crate::quackpack::util::to_url::ToUrl;

    #[test]
    fn dep_identity_display() {
        // Alias.
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            let identity = DepIdWithAlias::new("alias".into(), Identity::new("foo".into(), origin));
            let formatted = identity.to_string();
            assert_eq!(formatted, "foo#alias registry+https://localhost:9001/");
        }
        // No alias.
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            let identity = DepIdWithAlias::new("foo".into(), Identity::new("foo".into(), origin));
            let formatted = identity.to_string();
            assert_eq!(formatted, "foo registry+https://localhost:9001/");
        }
    }

    #[test]
    fn dep_identity_parse() {
        // Alias.
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            let identity = DepIdWithAlias::new("alias".into(), Identity::new("foo".into(), origin));
            let formatted = identity.to_string();
            let parsed = formatted.parse::<DepIdWithAlias>().unwrap();
            assert_eq!(parsed, identity);
        }
        // No alias.
        {
            let url = "https://localhost:9001".to_url().unwrap();
            let origin = Origin::for_registry(url);
            let identity = DepIdWithAlias::new("foo".into(), Identity::new("foo".into(), origin));
            let formatted = identity.to_string();
            let parsed = formatted.parse::<DepIdWithAlias>().unwrap();
            assert_eq!(parsed, identity);
        }
    }
}
