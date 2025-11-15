use std::collections::{HashMap, HashSet, VecDeque};

use anyhow::bail;

use crate::{QuackResult, StrId};

pub type FeatureName = StrId;
pub type PulledFeatures = Vec<FeatureName>;

#[derive(Debug)]
pub struct Features(HashMap<FeatureName, Vec<FeatureName>>);

impl Features {
    pub fn new(features: HashMap<FeatureName, Vec<FeatureName>>) -> QuackResult<Self> {
        Self::is_valid_features_map(&features)?;
        Ok(Self(features))
    }

    fn is_valid_features_map(features: &HashMap<FeatureName, Vec<FeatureName>>) -> QuackResult<()> {
        for (feature, pulled_features) in features {
            for pulled_feature in pulled_features {
                if !features.contains_key(pulled_feature) {
                    bail!(
                        "feature `{feature}` requires absent feature `{pulled_feature}`\n\
                           help: every feature needs to pull in some features, try adding `{pulled_feature}: []` to your manifest"
                    )
                }
            }
        }
        Ok(())
    }

    pub fn has_feature(&self, feature: FeatureName) -> bool {
        self.0.contains_key(&feature)
    }

    pub fn expand_features(
        &self,
        root_features: impl IntoIterator<Item = FeatureName>,
    ) -> QuackResult<PulledFeatures> {
        let mut already_visited = HashSet::new();
        let mut current_stack = root_features.into_iter().collect::<VecDeque<_>>();
        let mut result = vec![];
        while let Some(feature) = current_stack.pop_front() {
            if already_visited.contains(&feature) {
                continue;
            }
            let Some(pulled_features) = self.0.get(&feature) else {
                bail!("there is no such feature as `{feature}`")
            };
            let to_insert = pulled_features.iter().filter_map(|feature| {
                if already_visited.contains(feature) {
                    None
                } else {
                    Some(*feature)
                }
            });
            current_stack.extend(to_insert);
            already_visited.insert(feature);
            result.push(feature);
        }
        Ok(result)
    }

    pub fn as_map(&self) -> &HashMap<FeatureName, Vec<FeatureName>> {
        &self.0
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
        let mut from_a = features.expand_features([FeatureName::new("a")]).unwrap();
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
            .unwrap();
        from_a_and_b.sort();
        assert_eq!(from_a_and_b, from_a);

        let mut from_d = features.expand_features([FeatureName::new("d")]).unwrap();
        from_d.sort();
        let mut expected = [FeatureName::new("d"), FeatureName::new("e")];
        expected.sort();
        assert_eq!(from_d, expected);

        let mut from_d_and_f = features
            .expand_features([FeatureName::new("d"), FeatureName::new("f")])
            .unwrap();
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
            features.expand_features([FeatureName::new("c")]).unwrap(),
            [FeatureName::new("c")]
        );
    }

    #[test]
    fn no_features() {
        let features = Features::new(HashMap::new()).unwrap();
        assert!(features.expand_features([FeatureName::new("foo")]).is_err());
    }

    #[test]
    fn invalid_features() {
        assert!(Features::new(make_invalid_map()).is_err())
    }
}
