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
    levels_by_name: HashMap<String, LintLevel>,
}

impl Lints {
    /// Create a new [`Lints`] with given [`LintLevel`]s overrides.
    pub fn new(overrides: HashMap<String, LintLevel>) -> Self {
        Self {
            levels_by_name: overrides,
        }
    }

    /// Get the [`LintLevel`] for the given [`Lint`].
    ///
    /// If level was not overridden, the lint's default level is returned instead.
    pub fn level_for_lint(&self, lint: &Lint) -> LintLevel {
        self.levels_by_name
            .get(lint.name)
            .copied()
            .unwrap_or(lint.level)
    }
}
