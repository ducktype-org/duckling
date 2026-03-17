//! An implementation of the [Levenshtein distance](https://en.wikipedia.org/wiki/Levenshtein_distance).
use std::mem::swap;

/// Get the [Levenshtein distance](https://en.wikipedia.org/wiki/Levenshtein_distance) between `x`
/// and `y`.
pub fn distance(x: &str, y: &str) -> u32 {
    let word1: Vec<char> = x.chars().collect();
    let word2: Vec<char> = y.chars().collect();
    let m = x.len();
    let n = y.len();
    let mut v0 = Vec::from_iter(0..=n);
    let mut v1 = vec![0; n + 1];
    for (i, w1_letter) in word1.iter().enumerate().take(m) {
        v1[0] = i + 1;
        for (j, w2_letter) in word2.iter().enumerate().take(n) {
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
    v0[n].try_into().unwrap()
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
}
