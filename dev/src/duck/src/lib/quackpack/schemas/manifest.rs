use crate::quackpack::core::Version;
use crate::quackpack::schemas::OneEntryMap;
use std::collections::HashMap;
use std::fmt;

use serde::Deserialize;
use serde::de;
use serde_untagged::UntaggedEnumVisitor;

pub type Dependencies = HashMap<String, Dependency>;

#[derive(Debug, Deserialize)]
pub struct Manifest {
    pub metadata: Option<Metadata>,
    pub dependencies: Option<Dependencies>,
    pub dev_dependencies: Option<Dependencies>,
    pub features: Option<HashMap<String, Vec<String>>>,
    pub profiles: Option<HashMap<String, CompilerOptions>>,
}

#[derive(Debug, Deserialize)]
pub struct Metadata {
    pub version: Option<Version>,
    pub authors: Option<Vec<String>>,
    pub license: Option<String>,
    pub name: Option<String>,
    pub description: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct Dependency {
    pub version: Option<OredSemver>,
    pub source: Option<DependencySource>,
    pub features: Option<Vec<DependencyFeature>>,
    pub pinned: Option<bool>,
    pub conditions: Option<DependencyCondition>,
}

#[derive(Debug)]
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

#[derive(Debug)]
pub enum DependencySource {
    /// `Simple` variant overwrites `registry_url` for a given dependency.
    Simple(String),
    Detailed(DetailedSource),
}

impl DependencySource {
    pub fn has_git(&self) -> bool {
        match self {
            DependencySource::Simple(_) => false,
            DependencySource::Detailed(detailed_source) => detailed_source.has_git(),
        }
    }

    pub fn has_local(&self) -> bool {
        match self {
            DependencySource::Simple(_) => false,
            DependencySource::Detailed(detailed_source) => detailed_source.has_local(),
        }
    }

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

#[derive(Debug, Deserialize)]
pub struct DetailedSource {
    pub registry_url: Option<String>,
    pub name: Option<String>,
    pub path: Option<String>,
    pub git_url: Option<String>,
    pub tag: Option<String>,
    pub commit: Option<String>,
    pub branch: Option<String>,
}

impl DetailedSource {
    pub fn has_git(&self) -> bool {
        self.git_url.is_some()
    }

    pub fn has_local(&self) -> bool {
        self.path.is_some()
    }

    pub fn has_registry(&self) -> bool {
        self.registry_url.is_some()
    }
}

#[derive(Debug, Deserialize)]
pub struct DependencyCondition {
    pub package_features: Option<Vec<String>>,
}

#[derive(Debug, Deserialize)]
#[serde(transparent)]
pub struct DetailedFeature(pub OneEntryMap<String, DependencyCondition>);

#[derive(Debug)]
pub enum DependencyFeature {
    Simple(String),
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

#[derive(Debug, Deserialize)]
pub struct CompilerOptions {
    pub compiler_flags: Option<Vec<String>>,
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json;

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
