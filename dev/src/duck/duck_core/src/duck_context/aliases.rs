// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! User-defined subcommand aliases, as written in the `aliases:` table of the user config.
use std::collections::HashMap;

use serde::{Deserialize, de};

/// A single configuration alias.
/// Can be a string ("build --release"), or a list (["build", "--release"]).
#[derive(Debug)]
pub enum Alias {
    ToSplit(String),
    Splitted(Vec<String>),
}

impl Alias {
    pub fn split(&self) -> Vec<String> {
        match self {
            Self::ToSplit(joined) => joined.split(' ').map(String::from).collect(),
            Self::Splitted(splitted) => splitted.clone(),
        }
    }
}

impl<'de> de::Deserialize<'de> for Alias {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a string or a list of strings")
            .string(|alias| Ok(Self::ToSplit(alias.into())))
            .seq(|seq| seq.deserialize().map(Self::Splitted))
            .deserialize(deserializer)
    }
}

#[derive(Debug, Default, Deserialize)]
#[serde(transparent)]
pub struct Aliases(pub HashMap<String, Alias>);
