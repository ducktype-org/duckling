// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use crate::duck::driver::cli_args_preprocessing::levenshtein;

/// Get the closest match to the `key` withing a given range.
///
/// Returns `None` if there is no match in the given radius.
///
/// If there are many matches, returns the one with the smallest index.
pub fn get_closest_match<'a>(
    key: &str,
    values: &[&'a str],
    max_distance: Option<u32>,
) -> Option<&'a str> {
    values
        .iter()
        .filter_map(|value| {
            let distance = levenshtein::distance(key, value);
            let should_include = match max_distance {
                Some(max) => distance <= max,
                None => true,
            };
            should_include.then_some((*value, distance))
        })
        .min_by_key(|(_, distance)| *distance)
        .map(|(value, _)| value)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exact_match_is_always_the_best() {
        assert_eq!(
            "a",
            get_closest_match("a", &["a", "b", "aba"], None).unwrap()
        );
        assert_eq!(
            "a",
            get_closest_match("a", &["a", "b", "aba"], Some(0)).unwrap()
        );
        assert_eq!(
            "a",
            get_closest_match("a", &["a", "b", "aba"], Some(1)).unwrap()
        );
        assert_eq!(
            "a",
            get_closest_match("a", &["a", "b", "aba"], Some(u32::MAX)).unwrap()
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
            "kota",
            get_closest_match("kol", &["ala", "maciek", "kota", "pies"], None).unwrap(),
        );
    }
}
