// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! An implementation of the [Levenshtein distance](https://en.wikipedia.org/wiki/Levenshtein_distance),
//! and various helpers.

use std::mem::swap;

/// Get the [Levenshtein distance](https://en.wikipedia.org/wiki/Levenshtein_distance) between `x`
/// and `y`.
pub fn distance(x: &str, y: &str) -> u32 {
    let word1: Vec<char> = x.chars().collect();
    let word2: Vec<char> = y.chars().collect();
    let n = word2.len();
    let mut v0 = Vec::from_iter(0..=n);
    let mut v1 = vec![0; n + 1];
    for (i, w1_letter) in word1.iter().enumerate() {
        v1[0] = i + 1;
        for (j, w2_letter) in word2.iter().enumerate() {
            let deletion_cost = v0[j + 1] + 1;
            let insertion_cost = v1[j] + 1;
            let substitution_cost = v0[j] + (w1_letter != w2_letter) as usize;
            v1[j + 1] = *[deletion_cost, insertion_cost, substitution_cost]
                .iter()
                .min()
                .unwrap();
        }
        swap(&mut v0, &mut v1);
    }
    v0[n] as u32
}

/// Get the closest match to the `key` within a given range.
///
/// Returns `None` if there is no match in the given radius, or `values` is empty.
///
/// If several values have the same smallest distance, the first one is returned.
pub fn get_closest_match<'a>(
    key: &str,
    values: &[&'a str],
    max_distance: Option<u32>,
) -> Option<&'a str> {
    values
        .iter()
        .filter_map(|value| {
            let distance = distance(key, value);
            let should_include = max_distance.is_none_or(|max_distance| distance <= max_distance);
            should_include.then_some((*value, distance))
        })
        .min_by_key(|(_, distance)| *distance)
        .map(|(value, _)| value)
}

/// Get all closest targets to the `key`, which are no further than `max_distance` (in terms of
/// the Levenshtein distance).
pub fn find_closest_targets<'a>(
    key: &str,
    targets: &[&'a str],
    max_distance: Option<u32>,
) -> Vec<&'a str> {
    targets
        .iter()
        .fold(Vec::new(), |acc, target| {
            update_closest_targets(acc, target, key, max_distance)
        })
        .into_iter()
        .map(|(target, _)| target)
        .collect()
}

/// Update closest targets given the actual stack, target, and `max_fix_dist`.
///
/// This function will __only__ keep those values, which are (currently) closest to the target,
/// and all have exactly the same distance.
fn update_closest_targets<'a>(
    mut acc: Vec<(&'a str, u32)>,
    target: &'a str,
    key: &str,
    max_distance: Option<u32>,
) -> Vec<(&'a str, u32)> {
    let dist = distance(key, target);
    let has_too_large_distance = max_distance.is_some_and(|max_distance| dist > max_distance);
    if has_too_large_distance {
        return acc;
    }
    match acc.len() {
        0 => {
            acc.push((target, dist));
        }
        _ => match (dist < acc[0].1, dist == acc[0].1) {
            (true, false) => {
                acc.clear();
                acc.push((target, dist));
            }
            (false, true) => {
                acc.push((target, dist));
            }
            _ => {}
        },
    }
    acc
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_empty() {
        assert_eq!(distance("", ""), 0);
    }

    #[test]
    fn test_easy() {
        assert_eq!(distance("a", "a"), 0);
        assert_eq!(distance("", "xd"), 2);
        assert_eq!(distance("xd", ""), 2);
        assert_eq!(distance("ABCD", "AF"), 3);
        assert_eq!(distance("ABCD", "ABF"), 2);
        assert_eq!(distance("KOTA", "KOTA"), 0);
    }

    #[test]
    fn test_wikipedia() {
        assert_eq!(distance("kitten", "sitting"), 3);
        assert_eq!(distance("uninformed", "uniformed"), 1);
    }

    #[test]
    fn weird() {
        assert_eq!(distance("naive", "naïve"), 1);
        assert_eq!(distance("café", "café"), 0);
        assert_eq!(distance("cafe", "café"), 1);
        assert_eq!(distance("ą", "a"), 1);
    }

    #[test]
    fn exact_match_is_always_the_best() {
        assert_eq!(Some("a"), get_closest_match("a", &["a", "b", "aba"], None));
        assert_eq!(
            Some("a"),
            get_closest_match("a", &["a", "b", "aba"], Some(0))
        );
        assert_eq!(
            Some("a"),
            get_closest_match("a", &["a", "b", "aba"], Some(1))
        );
        assert_eq!(
            Some("a"),
            get_closest_match("a", &["a", "b", "aba"], Some(u32::MAX))
        );
    }

    #[test]
    fn none_tests() {
        assert_eq!(
            None,
            get_closest_match("ab", &["ala", "ma", "kota"], Some(1)),
        );
    }

    #[test]
    fn multiple_matches_smallest_distance() {
        assert_eq!(
            Some("kota"),
            get_closest_match("kol", &["ala", "maciek", "kota", "pies"], None)
        );
        // This tests that we return the first index if there are multiple matches.
        assert_eq!(Some("aa"), get_closest_match("ab", &["aa", "bb"], None));
    }
}
