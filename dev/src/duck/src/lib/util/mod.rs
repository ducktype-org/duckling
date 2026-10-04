//! Common quackpack and duck functions.

use std::collections::{HashMap, HashSet};
pub mod command_ext;
pub mod dependency_graph;
pub mod env;
pub mod error;
pub mod extend;
pub mod extract;
pub mod file_locks;
pub mod hash;
pub mod hex;
pub mod once_lock_ext;
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
pub trait Pluralize {
    /// Determine whether `self` is plural or singular.
    fn is_plural(&self) -> bool;

    /// Return `s` if plural else an empty word.
    /// Obviously in english there are words for which the plural form is created differently to simply adding `s`.
    /// But in the project there are currently no such words displayed to the user.
    fn s_if_plural(&self) -> &'static str {
        if self.is_plural() { "s" } else { "" }
    }

    /// Return `were` if plural else `was`.
    fn was_or_were(&self) -> &'static str {
        if self.is_plural() { "were" } else { "was" }
    }
}

impl<T> Pluralize for Vec<T> {
    fn is_plural(&self) -> bool {
        self.len() != 1
    }
}

impl<T> Pluralize for &[T] {
    fn is_plural(&self) -> bool {
        self.len() != 1
    }
}

impl<K, V> Pluralize for HashMap<K, V> {
    fn is_plural(&self) -> bool {
        self.len() != 1
    }
}

impl<T> Pluralize for HashSet<T> {
    fn is_plural(&self) -> bool {
        self.len() != 1
    }
}

impl Pluralize for usize {
    fn is_plural(&self) -> bool {
        *self != 1
    }
}
