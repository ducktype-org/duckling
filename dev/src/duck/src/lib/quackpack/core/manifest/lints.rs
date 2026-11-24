// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Lint configuration.

use std::collections::HashMap;

use crate::quackpack::core::lints::{Lint, LintLevel};

#[derive(Debug, Clone, Default)]
/// QuackPack's lints' configuration.
pub struct Lints {
    pub duck_lints: HashMap<String, LintLevel>,
}

impl Lints {
    /// Get the [`LintLevel`] for the given [`Lint`].
    ///
    /// If level was not overridden, the lint's default level is returned instead.
    pub fn level_for_duck_lint(&self, lint: &Lint) -> LintLevel {
        self.duck_lints
            .get(lint.name)
            .copied()
            .unwrap_or(lint.level)
    }
}
