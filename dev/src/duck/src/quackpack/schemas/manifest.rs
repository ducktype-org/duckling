use std::collections::{BTreeMap, BTreeSet};
use std::fmt;

use serde::Deserialize;
use serde::de;
use serde_untagged::UntaggedEnumVisitor;

#[derive(Debug, Deserialize)]
pub struct Manifest {
    pub metadata: Option<Metadata>,
    pub dependencies: Option<BTreeMap<String, Dependency>>,
    pub dev_dependencies: Option<BTreeMap<String, Dependency>>,
    pub features: Option<BTreeMap<String, Vec<String>>>,
    pub targets: Option<BTreeMap<String, CompilerOptions>>,
    pub profiles: Option<BTreeMap<String, CompilerOptions>>,

    #[serde(skip)]
    pub _unused: BTreeSet<String>,
}

#[derive(Debug, Deserialize)]
pub struct Metadata {
    pub version: Option<semver::Version>,
    pub authors: Option<Vec<String>>,
    pub license: Option<String>,
    pub name: Option<String>,
    pub description: Option<String>,
    pub language: Option<semver::Version>,
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
pub struct OredSemver(pub Vec<semver::Version>);

impl<'de> Deserialize<'de> for OredSemver {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        struct SeqOrSplit;
        impl<'de> de::Visitor<'de> for SeqOrSplit {
            type Value = Vec<semver::Version>;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a string or a sequence of strings")
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                v.split(" or ")
                    .map(|x| semver::Version::parse(x.trim()))
                    .collect::<Result<Vec<_>, _>>()
                    .map_err(|e| de::Error::custom(e))
            }

            fn visit_seq<A>(self, mut seq: A) -> Result<Self::Value, A::Error>
            where
                A: de::SeqAccess<'de>,
            {
                let mut values = vec![];
                while let Some(next) = seq.next_element::<semver::Version>()? {
                    values.push(next);
                }
                Ok(values)
            }
        }
        deserializer.deserialize_any(SeqOrSplit).map(Self)
    }
}

#[derive(Debug)]
pub enum DependencySource {
    Simple(String),
    Detailed(DetailedSource),
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

#[derive(Debug, Deserialize)]
pub struct DependencyCondition {
    pub system: Option<Vec<String>>,
    pub arch: Option<Vec<String>>,
    pub package_features: Option<Vec<String>>,
}

#[derive(Debug, Deserialize)]
#[serde(transparent)]
pub struct DetailedFeature(pub BTreeMap<String, DependencyCondition>);

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
        assert_eq!(x.0, [semver::Version::new(1, 0, 0)]);

        let x = serde_json::from_str::<OredSemver>(r#"" 1.0.0 or  1.1.0 or  2.0.0  ""#).unwrap();
        assert_eq!(
            x.0,
            [
                semver::Version::new(1, 0, 0),
                semver::Version::new(1, 1, 0),
                semver::Version::new(2, 0, 0)
            ]
        );

        let y = serde_json::from_str::<OredSemver>(r#"["1.0.0", "1.1.0", "2.0.0"]"#).unwrap();
        assert_eq!(y.0, x.0);
    }
}
