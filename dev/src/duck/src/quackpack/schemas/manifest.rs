use crate::core::Version;
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
    pub version: Option<Version>,
    pub authors: Option<Vec<String>>,
    pub license: Option<String>,
    pub name: Option<String>,
    pub description: Option<String>,
    pub language: Option<Version>,
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
pub struct OredSemver(pub NonEmptyVec<Version>);

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
                if values.is_empty() {
                    Err(de::Error::custom("expected at least one version"))
                } else {
                    Ok(values)
                }
            }
        }
        // SAFETY: `visit_seq` manually checks for empty vectors, and `visit_str` implementation combined
        // with `Version::from_str` implementation assumes, that it's not empty.
        deserializer
            .deserialize_any(SeqOrSplit)
            .map(NonEmptyVec)
            .map(Self)
    }
}

#[derive(Debug)]
pub enum DependencySource {
    Simple(String),
    Detailed(DetailedSource),
}

#[derive(Debug)]
pub struct NonEmptyVec<T>(Vec<T>);

impl<T> AsRef<Vec<T>> for NonEmptyVec<T> {
    fn as_ref(&self) -> &Vec<T> {
        &self.0
    }
}

impl<T> From<NonEmptyVec<T>> for Vec<T> {
    fn from(value: NonEmptyVec<T>) -> Self {
        value.0
    }
}

impl<'de, T> de::Deserialize<'de> for NonEmptyVec<T>
where
    T: de::Deserialize<'de>,
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        let inner = <Vec<T>>::deserialize(deserializer)?;
        if inner.is_empty() {
            Err(de::Error::custom("expected non-empty list"))
        } else {
            Ok(Self(inner))
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

#[derive(Debug, Deserialize)]
pub struct DependencyCondition {
    pub system: Option<NonEmptyVec<String>>,
    pub arch: Option<NonEmptyVec<String>>,
    pub package_features: Option<NonEmptyVec<String>>,
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
        assert_eq!(Vec::from(x.0), [Version::new(1, 0, 0)]);

        let x = serde_json::from_str::<OredSemver>(r#"" 1.0.0 or  1.1.0 or  2.0.0  ""#).unwrap();
        assert_eq!(
            Vec::from(x.0),
            [
                Version::new(1, 0, 0),
                Version::new(1, 1, 0),
                Version::new(2, 0, 0)
            ]
        );

        let x = serde_json::from_str::<OredSemver>(r#"" 1.0.0 or  1.1.0 or  2.0.0  ""#).unwrap();
        let y = serde_json::from_str::<OredSemver>(r#"["1.0.0", "1.1.0", "2.0.0"]"#).unwrap();
        assert_eq!(Vec::from(x.0), Vec::from(y.0));
    }

    #[test]
    fn test_empty_ored_semver_() {
        assert!(serde_json::from_str::<OredSemver>(r#""""#).is_err());
        assert!(serde_json::from_str::<OredSemver>(r#""  ""#).is_err());
        assert!(serde_json::from_str::<OredSemver>(r#""  or ""#).is_err());
        assert!(serde_json::from_str::<OredSemver>(r#"[]"#).is_err());
    }
}
