use std::collections::{HashMap, HashSet, VecDeque};

use anyhow::bail;

use crate::{QuackResult, StrId};

#[derive(Debug)]
pub struct Features(HashMap<StrId, Vec<StrId>>);

impl Features {
    pub fn new(features: HashMap<StrId, Vec<StrId>>) -> QuackResult<Self> {
        Self::is_valid_features_map(&features)?;
        Ok(Self(features))
    }

    fn is_valid_features_map(features: &HashMap<StrId, Vec<StrId>>) -> QuackResult<()> {
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

    pub fn has_feature(&self, feature: StrId) -> bool {
        self.0.contains_key(&feature)
    }

    pub fn expand_features(
        &self,
        root_features: impl IntoIterator<Item = StrId>,
    ) -> QuackResult<Vec<StrId>> {
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

    pub fn as_map(&self) -> &HashMap<StrId, Vec<StrId>> {
        &self.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::StrId;

    fn make_features_map() -> HashMap<StrId, Vec<StrId>> {
        //         a        d     f
        //        / \        \  /   \
        //       c   b        e      g
        //            \
        //             c
        [
            (StrId::new("a"), vec![StrId::new("b"), StrId::new("c")]),
            (StrId::new("b"), vec![StrId::new("c")]),
            (StrId::new("c"), vec![]),
            (StrId::new("d"), vec![StrId::new("e")]),
            (StrId::new("e"), vec![]),
            (StrId::new("f"), vec![StrId::new("e"), StrId::new("g")]),
            (StrId::new("g"), vec![]),
        ]
        .into()
    }
    fn make_invalid_map() -> HashMap<StrId, Vec<StrId>> {
        [(StrId::new("a"), vec![StrId::new("b")])].into()
    }

    #[test]
    fn test_valid() {
        let features = Features::new(make_features_map());
        assert!(features.is_ok());
        let features = features.unwrap();
        let mut from_a = features.expand_features([StrId::new("a")]).unwrap();
        from_a.sort();
        let mut expected = [StrId::new("a"), StrId::new("b"), StrId::new("c")];
        expected.sort();
        assert_eq!(from_a, expected);

        let mut from_a_and_b = features
            .expand_features([StrId::new("b"), StrId::new("a")])
            .unwrap();
        from_a_and_b.sort();
        assert_eq!(from_a_and_b, from_a);

        let mut from_d = features.expand_features([StrId::new("d")]).unwrap();
        from_d.sort();
        let mut expected = [StrId::new("d"), StrId::new("e")];
        expected.sort();
        assert_eq!(from_d, expected);

        let mut from_d_and_f = features
            .expand_features([StrId::new("d"), StrId::new("f")])
            .unwrap();
        from_d_and_f.sort();
        let mut expected = [
            StrId::new("d"),
            StrId::new("e"),
            StrId::new("f"),
            StrId::new("g"),
        ];
        expected.sort();
        assert_eq!(from_d_and_f, expected);

        assert_eq!(
            features.expand_features([StrId::new("c")]).unwrap(),
            [StrId::new("c")]
        );
    }

    #[test]
    fn no_features() {
        let features = Features::new(HashMap::new()).unwrap();
        assert!(features.expand_features([StrId::new("foo")]).is_err());
    }

    #[test]
    fn invalid_features() {
        assert!(Features::new(make_invalid_map()).is_err())
    }
}
