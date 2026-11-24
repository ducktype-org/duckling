// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Multiple different miscellaneous utilities used throughout the project.
//! Most important submodules are:
//! - [`filesystem`]: operations on files, directories and paths; custom filelocks; custom iterators over directory contents
//! - [`interning`]: interning string and URLs
//! - [`yaml_config`]: abstraction over a configuration file with deserialization from YAML

use std::collections::{HashMap, HashSet};
pub mod command_ext;
pub mod duckling_consts;
pub mod edit_distance;
pub mod extend;
pub mod extract;
pub mod filesystem;
pub mod hash;
pub mod hex;
pub mod indent;
pub mod interning;
pub mod once_ext;
pub mod set_once;
pub mod yaml_config;

pub mod test_utils;

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
