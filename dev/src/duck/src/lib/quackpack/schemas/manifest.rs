//! Local manifest schemas.
use std::collections::HashMap;
use std::fmt;
use std::path::PathBuf;

use itertools::Itertools;
use serde::{Deserialize, Serialize, de, ser};
use serde_untagged::UntaggedEnumVisitor;
use serde_with::skip_serializing_none;

use crate::quackpack::core::Version;
use crate::quackpack::schemas::OneEntryMap;

pub type Dependencies = HashMap<String, Dependency>;

#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
/// Schema of the [`quackconfig.yaml`](crate::quackpack::core::PackageLoader::MANIFEST_NAME) file.
/// Also used by script frontmatters.
pub struct Manifest {
    /// `metadata:` root field.
    pub metadata: Option<Metadata>,
    /// `dependencies:` root field.
    pub dependencies: Option<Dependencies>,
    /// `dev_dependencies:` root field.
    pub dev_dependencies: Option<Dependencies>,
    /// `features:` root field.
    pub features: Option<HashMap<String, Vec<String>>>,
    /// `profiles:` root field.
    pub profiles: Option<HashMap<String, Profile>>,
    /// `import:` root field.
    /// Only used by frontmatters.
    pub import: Option<PathBuf>,
    /// `venv:` root field.
    pub venv: Option<VenvConfig>,
}

impl Manifest {
    /// Checks whether at most the [`Manifest::import`] field is not [`None`].
    pub fn is_just_import_or_empty(&self) -> bool {
        self.metadata.is_none()
            && self.features.is_none()
            && self.dependencies.is_none()
            && self.dev_dependencies.is_none()
            && self.profiles.is_none()
    }

    /// Checks whether fields only allowed in frontmatters are present and that `import` field is absent.
    pub fn fields_disallowed_in_expanded_frontmatter(&self) -> Vec<&'static str> {
        let mut result = vec![];
        if self.metadata.is_some() {
            result.push("metadata");
        }
        if self.features.is_some() {
            result.push("features");
        }
        if self.import.is_some() {
            result.push("import");
        }
        if self.venv.is_some() {
            result.push("venv");
        }
        result
    }

    /// Remove a dependency.
    pub fn remove_dependency(&mut self, name: &str) -> DependencyRemoved {
        if self
            .dependencies
            .as_mut()
            .and_then(|deps| deps.remove_entry(name))
            .is_none()
        {
            // There is no such dependency. Check if there is a dev-dependency instead.
            if let Some(ref dev_deps) = self.dev_dependencies
                && dev_deps.contains_key(name)
            {
                return DependencyRemoved::NoDependencyButDevDepExists;
            } else {
                return DependencyRemoved::NoDependency;
            }
        }
        if let Some(ref map) = self.dependencies
            && map.is_empty()
        {
            self.dependencies = None;
        }
        DependencyRemoved::Yes
    }

    /// Remove a dev-dependency.
    pub fn remove_dev_dependency(&mut self, name: &str) -> DependencyRemoved {
        if self
            .dev_dependencies
            .as_mut()
            .and_then(|deps| deps.remove_entry(name))
            .is_none()
        {
            return DependencyRemoved::NoDependency;
        }
        if let Some(ref map) = self.dev_dependencies
            && map.is_empty()
        {
            self.dev_dependencies = None;
        }
        DependencyRemoved::Yes
    }

    pub fn add_dependency(&mut self, name: String, dependency: Dependency) -> DependencyAdded {
        let dependencies = self.dependencies.get_or_insert(Dependencies::new());
        if dependencies.contains_key(&name) {
            DependencyAdded::AlreadyExists
        } else {
            dependencies.insert(name, dependency);
            DependencyAdded::Yes
        }
    }
}

/// Marker struct for a removal of a dependency.
pub enum DependencyRemoved {
    Yes,
    NoDependency,
    NoDependencyButDevDepExists,
}

/// Marker struct for an addition of a dependency.
pub enum DependencyAdded {
    Yes,
    AlreadyExists,
}

#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
/// Schema of the `metadata:` table.
pub struct Metadata {
    /// Package's name.
    pub name: Option<String>,
    /// Version of the package.
    pub version: Option<Version>,
    /// Package's authors.
    pub authors: Option<Vec<String>>,
    /// Package's license.
    pub license: Option<String>,
    /// Package's description.
    pub description: Option<String>,
}

#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
/// Single dependency of the package.
pub struct Dependency {
    /// Dependency's version.
    pub version: Option<OredSemver>,
    /// Dependency's source.
    pub source: Option<DependencySource>,
    /// Dependency's features.
    pub features: Option<Vec<DependencyFeature>>,
    /// Whether this dependency is pinned.
    pub pinned: Option<bool>,
    /// Dependency's conditions: any has to be true in order to enable this dependency.
    pub conditions: Option<DependencyCondition>,
}

#[derive(Clone, Debug)]
/// A dependency versions which can be present in one of the two ways:
/// 1. as a list of strings, where each string is a valid version,
/// 2. as a string of versions which are separated by an `" or "` keyword (extra spaces are ignored).
pub struct OredSemver(pub Vec<Version>);

impl<'de> Deserialize<'de> for OredSemver {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        struct SeqOrSplit;
        impl<'de> de::Visitor<'de> for SeqOrSplit {
            type Value = Vec<Version>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("an ored semver string or a list of semver strings")
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                v.split(" or ")
                    .map(|x| x.trim().parse())
                    .collect::<Result<Vec<_>, _>>()
                    .map_err(|e| de::Error::custom(e))
            }

            fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
            where
                A: de::SeqAccess<'de>,
            {
                let mut values = vec![];
                while let Some(next) = seq.next_element::<Version>()? {
                    values.push(next);
                }
                Ok(values)
            }
        }
        // SAFETY: `visit_seq` manually checks for empty vectors, and `visit_str` implementation combined
        // with `Version::from_str` implementation assumes, that it's not empty.
        deserializer.deserialize_any(SeqOrSplit).map(Self)
    }
}

impl Serialize for OredSemver {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        let semver_string = self.0.iter().map(ToString::to_string).join(" or ");
        serializer.serialize_str(&semver_string)
    }
}

#[derive(Clone, Debug)]
/// A dependency's source.
pub enum DependencySource {
    /// `Simple` variant overwrites `registry_url` for a given dependency.
    Simple(String),
    /// More detailed source.
    Detailed(DetailedSource),
}

impl DependencySource {
    /// Whether the required git-related fields are present.
    pub fn has_git(&self) -> bool {
        match self {
            DependencySource::Simple(_) => false,
            DependencySource::Detailed(detailed_source) => detailed_source.has_git(),
        }
    }

    /// Whether the required local-related fields are present.
    pub fn has_local(&self) -> bool {
        match self {
            DependencySource::Simple(_) => false,
            DependencySource::Detailed(detailed_source) => detailed_source.has_local(),
        }
    }

    /// Whether the required registry-related fields are present.
    pub fn has_registry(&self) -> bool {
        match self {
            DependencySource::Simple(_) => true,
            DependencySource::Detailed(detailed_source) => {
                // Either we have explicit `registry_url`,
                // or we have explicit `alias` without explicit `path` or `git_url` (implicit default registry).
                detailed_source.has_registry()
                    || (detailed_source.name.is_some()
                        && !detailed_source.has_git()
                        && !detailed_source.has_local())
            }
        }
    }
}

impl<'de> de::Deserialize<'de> for DependencySource {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        UntaggedEnumVisitor::new()
            .expecting("a semver string or a detailed dependency")
            .string(|value| Ok(DependencySource::Simple(value.to_owned())))
            .map(|value| value.deserialize().map(DependencySource::Detailed))
            .deserialize(deserializer)
    }
}

impl Serialize for DependencySource {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        match self {
            DependencySource::Simple(s) => serializer.serialize_str(s),
            DependencySource::Detailed(detailed_source) => detailed_source.serialize(serializer),
        }
    }
}

#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
/// A detailed source of a dependency.
pub struct DetailedSource {
    /// Overridden registry url.
    pub registry_url: Option<String>,
    /// This dependency is actually an alias; download package pointed by `name`.
    pub name: Option<String>,
    /// Path to the local dependency.
    pub path: Option<PathBuf>,
    /// Url for the git dependency.
    pub git_url: Option<String>,
    /// Git's tag.
    pub tag: Option<String>,
    /// Git's commit.
    pub commit: Option<String>,
    /// Git's branch.
    pub branch: Option<String>,
}

impl DetailedSource {
    /// Whether crucial git fields are present.
    pub fn has_git(&self) -> bool {
        self.git_url.is_some()
    }

    /// Whether crucial local fields are present.
    pub fn has_local(&self) -> bool {
        self.path.is_some()
    }

    /// Whether crucial registry fields are present.
    pub fn has_registry(&self) -> bool {
        self.registry_url.is_some()
    }
}

#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
/// Conditions, from which any has to be true, in order to enable this dependency.
pub struct DependencyCondition {
    /// Enable this dependency/feature if we build the root package with at least one of the
    /// specified features. (Omitted field means always build)
    pub package_features: Option<Vec<String>>,
}

#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case", transparent)]
/// A feature + its conditions.
pub struct DetailedFeature(pub OneEntryMap<String, DependencyCondition>);

#[derive(Clone, Debug)]
/// A general dependency feature.
pub enum DependencyFeature {
    /// Just a feature.
    Simple(String),
    /// A detailed feature.
    Detailed(DetailedFeature),
}

impl<'de> de::Deserialize<'de> for DependencyFeature {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        UntaggedEnumVisitor::new()
            .expecting("a semver string or a detailed dependency")
            .string(|value| Ok(DependencyFeature::Simple(value.to_owned())))
            .map(|value| value.deserialize().map(DependencyFeature::Detailed))
            .deserialize(deserializer)
    }
}

impl Serialize for DependencyFeature {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        match self {
            DependencyFeature::Simple(simple) => serializer.serialize_str(simple),
            DependencyFeature::Detailed(detailed_feature) => detailed_feature.serialize(serializer),
        }
    }
}

#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, Serialize)]
#[serde(rename_all = "kebab-case")]
/// A single compilation profile.
pub struct Profile {
    /// Optimization level.
    pub opt_level: Option<OptLevel>,
    /// Whether should we emit DVM bytecode.
    pub dvm_bytecode: Option<bool>,
    /// Whether to use incremental compilation.
    pub incremental: Option<bool>,
    /// Whether to link C STD.
    pub c_std: Option<bool>,
    /// Whether this profile inherits other profile.
    pub inherits: Option<String>,
}

#[derive(Clone, Debug)]
pub enum OptLevel {
    Number(u32),
    String(String),
}

impl<'de> de::Deserialize<'de> for OptLevel {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        UntaggedEnumVisitor::new()
            .expecting("a non-negative number or a string")
            .u32(|n| Ok(OptLevel::Number(n)))
            .string(|s| Ok(OptLevel::String(s.to_string())))
            .deserialize(deserializer)
    }
}

impl Serialize for OptLevel {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        match self {
            OptLevel::Number(n) => serializer.serialize_u32(*n),
            OptLevel::String(s) => serializer.serialize_str(s),
        }
    }
}

#[skip_serializing_none]
#[derive(Clone, Debug, Deserialize, Serialize)]
/// A venv's configuration.
#[serde(rename_all = "kebab-case")]
pub struct VenvConfig {
    pub storage_path: Option<PathBuf>,
    pub ephemeral: Option<bool>,
    pub expose_freezefile: Option<bool>,
}

#[cfg(test)]
mod tests {
    use serde_json;

    use super::*;

    #[test]
    fn test_ored_semver_deserialization() {
        let x = serde_json::from_str::<OredSemver>(r#""1.0.0""#).unwrap();
        assert_eq!(x.0, [Version::new(1, 0, 0)]);

        let x = serde_json::from_str::<OredSemver>(r#"" 1.0.0 or  1.1.0 or  2.0.0  ""#).unwrap();
        assert_eq!(
            x.0,
            [
                Version::new(1, 0, 0),
                Version::new(1, 1, 0),
                Version::new(2, 0, 0)
            ]
        );

        let y = serde_json::from_str::<OredSemver>(r#"["1.0.0", "1.1.0", "2.0.0"]"#).unwrap();
        assert_eq!(x.0, y.0);
    }

    #[test]
    fn test_ored_semver_serialization() {
        let x =
            serde_json::to_string::<OredSemver>(&OredSemver(vec![Version::new(1, 0, 0)])).unwrap();
        assert_eq!(x, "\"1.0.0\"");

        let x = serde_json::to_string::<OredSemver>(&OredSemver(vec![
            Version::new(1, 0, 0),
            Version::new(1, 1, 0),
            Version::new(2, 0, 0),
        ]))
        .unwrap();
        assert_eq!(x, "\"1.0.0 or 1.1.0 or 2.0.0\"");
    }

    #[test]
    fn test_empty_ored_semver_() {
        assert_eq!(
            serde_json::from_str::<OredSemver>(r#""""#)
                .unwrap_err()
                .to_string(),
            "cannot parse integer from empty string at line 1 column 2"
        );
        assert_eq!(
            serde_json::from_str::<OredSemver>(r#""  ""#)
                .unwrap_err()
                .to_string(),
            "cannot parse integer from empty string at line 1 column 4"
        );
        assert_eq!(
            serde_json::from_str::<OredSemver>(r#""  or ""#)
                .unwrap_err()
                .to_string(),
            "cannot parse integer from empty string at line 1 column 7"
        );
    }
}
