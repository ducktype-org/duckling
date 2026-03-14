//! Root package features handling.
use std::collections::{HashMap, HashSet, VecDeque};

use crate::{QuackError, QuackResult, StrId, qp_bail};

/// Name of a feature.
pub type FeatureName = StrId;
/// List of feature names that have been pulled in.
pub type PulledFeatures = HashSet<FeatureName>;

#[derive(Clone, Debug)]
/// Features exposed by a root package we are working on.
pub struct Features(HashMap<FeatureName, Vec<FeatureName>>);

impl Features {
    /// Create a [`Features`], validating that all referenced features exist.
    pub fn new(features: HashMap<FeatureName, Vec<FeatureName>>) -> QuackResult<Self> {
        Self::is_valid_features_map(&features)?;
        Ok(Self(features))
    }

    /// Checks, whether `features` is a valid features map (i.e. all values in `Vec`s exist as
    /// keys).
    fn is_valid_features_map(features: &HashMap<FeatureName, Vec<FeatureName>>) -> QuackResult<()> {
        for (feature, pulled_features) in features {
            for pulled_feature in pulled_features {
                if !features.contains_key(pulled_feature) {
                    qp_bail!(
                        "the feature `{feature}` requires an absent feature `{pulled_feature}`\n\
                         help: every feature needs to pull in some features, try adding \
                         `{pulled_feature}: []` to the your manifest"
                    )
                }
            }
        }
        Ok(())
    }

    /// Check, if a feature exists.
    pub fn has_feature(&self, feature: FeatureName) -> bool {
        self.0.contains_key(&feature)
    }

    /// Expand features by recursively pulling in all dependent features.
    pub fn expand_features(
        &self,
        root_features: impl IntoIterator<Item = FeatureName>,
    ) -> QuackResult<PulledFeatures> {
        let mut visited = HashSet::new();
        let mut current_stack = root_features.into_iter().collect::<VecDeque<_>>();
        while let Some(feature) = current_stack.pop_front() {
            if visited.contains(&feature) {
                continue;
            }
            let Some(pulled_features) = self.0.get(&feature) else {
                qp_bail!("there is no such feature as `{feature}`")
            };
            let to_insert = pulled_features.iter().filter_map(|feature| {
                if visited.contains(feature) {
                    None
                } else {
                    Some(*feature)
                }
            });
            current_stack.extend(to_insert);
            visited.insert(feature);
        }
        Ok(visited)
    }

    /// Get the underlying feature map.
    pub fn all_features(&self) -> &HashMap<FeatureName, Vec<FeatureName>> {
        &self.0
    }
}

impl TryFrom<HashMap<String, Vec<String>>> for Features {
    type Error = QuackError;

    fn try_from(value: HashMap<String, Vec<String>>) -> Result<Self, Self::Error> {
        Self::new(
            value
                .into_iter()
                .map(|(k, v)| (k.into(), v.into_iter().map(Into::into).collect()))
                .collect(),
        )
    }
}

impl From<Features> for HashMap<String, Vec<String>> {
    fn from(value: Features) -> Self {
        let map = value.0;
        map.into_iter()
            .map(|(k, v)| (k.into(), v.into_iter().map(Into::into).collect()))
            .collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_features_map() -> HashMap<FeatureName, Vec<FeatureName>> {
        //         a        d     f
        //        / \        \  /   \
        //       c   b        e      g
        //            \
        //             c
        [
            (
                FeatureName::new("a"),
                vec![FeatureName::new("b"), FeatureName::new("c")],
            ),
            (FeatureName::new("b"), vec![FeatureName::new("c")]),
            (FeatureName::new("c"), vec![]),
            (FeatureName::new("d"), vec![FeatureName::new("e")]),
            (FeatureName::new("e"), vec![]),
            (
                FeatureName::new("f"),
                vec![FeatureName::new("e"), FeatureName::new("g")],
            ),
            (FeatureName::new("g"), vec![]),
        ]
        .into()
    }
    fn make_invalid_map() -> HashMap<FeatureName, Vec<FeatureName>> {
        [(FeatureName::new("a"), vec![FeatureName::new("b")])].into()
    }

    #[test]
    fn test_valid() {
        let features = Features::new(make_features_map());
        assert!(features.is_ok());
        let features = features.unwrap();
        let mut from_a = features
            .expand_features([FeatureName::new("a")])
            .unwrap()
            .into_iter()
            .collect::<Vec<_>>();
        from_a.sort();
        let mut expected = [
            FeatureName::new("a"),
            FeatureName::new("b"),
            FeatureName::new("c"),
        ];
        expected.sort();
        assert_eq!(from_a, expected);

        let mut from_a_and_b = features
            .expand_features([FeatureName::new("b"), FeatureName::new("a")])
            .unwrap()
            .into_iter()
            .collect::<Vec<_>>();
        from_a_and_b.sort();
        assert_eq!(from_a_and_b, from_a);

        let mut from_d = features
            .expand_features([FeatureName::new("d")])
            .unwrap()
            .into_iter()
            .collect::<Vec<_>>();
        from_d.sort();
        let mut expected = [FeatureName::new("d"), FeatureName::new("e")];
        expected.sort();
        assert_eq!(from_d, expected);

        let mut from_d_and_f = features
            .expand_features([FeatureName::new("d"), FeatureName::new("f")])
            .unwrap()
            .into_iter()
            .collect::<Vec<_>>();
        from_d_and_f.sort();
        let mut expected = [
            FeatureName::new("d"),
            FeatureName::new("e"),
            FeatureName::new("f"),
            FeatureName::new("g"),
        ];
        expected.sort();
        assert_eq!(from_d_and_f, expected);

        assert_eq!(
            features
                .expand_features([FeatureName::new("c")])
                .unwrap()
                .into_iter()
                .collect::<Vec<_>>(),
            [FeatureName::new("c")]
        );
    }

    #[test]
    fn no_features() {
        let features = Features::new(HashMap::new()).unwrap();
        assert_eq!(
            features
                .expand_features([FeatureName::new("foo")])
                .unwrap_err()
                .to_string(),
            "there is no such feature as `foo`"
        );
    }

    #[test]
    fn invalid_features() {
        assert_eq!(
            Features::new(make_invalid_map()).unwrap_err().to_string(),
            "the feature `a` requires an absent feature `b`
help: every feature needs to pull in some features, try adding `b: []` to the your manifest"
        );
    }
}
