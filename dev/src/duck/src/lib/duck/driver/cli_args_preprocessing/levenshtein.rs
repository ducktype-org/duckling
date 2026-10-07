// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! An implementation of the [Levenshtein distance](https://en.wikipedia.org/wiki/Levenshtein_distance).
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
}
