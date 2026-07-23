//! Common quackpack and duck functions.

use std::collections::{HashMap, HashSet};
pub mod command_ext;
pub mod env;
pub mod error;
pub mod extend;
pub mod extract;
pub mod file_locks;
pub mod hash;
pub mod hex;
pub mod path_ops_ext;
pub mod set_once;
pub mod yaml_config;

#[cfg(test)]
pub mod test_utils;

/// English localization helper trait.
pub trait DescriptionWithAnArticle {
    /// Return a valid description with an appropriate article.
    fn desc_with_article(&self) -> &'static str;
}

/// English pluralization helper trait.
pub trait IsPlural {
    /// Return `s` if the data structure has 0 or at least 2 elements, otherwise return ``.
    /// Obviously in english there are words for which the plural form is created differently to simply adding `s`.
    /// But in the project there are currently no such words displayed to the user.
    fn s_if_plural(&self) -> &'static str;
}

impl<T> IsPlural for Vec<T> {
    fn s_if_plural(&self) -> &'static str {
        if self.len() == 1 { "" } else { "s" }
    }
}

impl<T> IsPlural for &[T] {
    fn s_if_plural(&self) -> &'static str {
        if self.len() == 1 { "" } else { "s" }
    }
}

impl<K, V> IsPlural for HashMap<K, V> {
    fn s_if_plural(&self) -> &'static str {
        if self.len() == 1 { "" } else { "s" }
    }
}

impl<T> IsPlural for HashSet<T> {
    fn s_if_plural(&self) -> &'static str {
        if self.len() == 1 { "" } else { "s" }
    }
}
