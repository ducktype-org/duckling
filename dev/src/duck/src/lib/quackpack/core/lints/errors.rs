// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various errors created when parsing the [`manifest`].
//!
//! They are simple diagnostics, not associated with any lint, and are not checked when running
//! [`run_lint_passes`].
//!
//! [`manifest`]: crate::quackpack::core::parse
//! [`run_lint_passes`]: super::run_lint_passes

use super::{Diagnostic, macros};
use crate::util::Pluralize;
use crate::{DuckContext, QuackResult, qp_bail};

#[derive(Debug, Default)]
/// Errors encountered when parsing the manifest.
pub struct Errors {
    errors: Vec<Box<dyn Diagnostic + 'static>>,
}

impl Errors {
    /// Create a new [`Errors`].
    pub fn new() -> Self {
        Self::default()
    }

    /// Register a new [`Diagnostic`] to be emitted later.
    pub fn push<T: Diagnostic + 'static>(&mut self, error: T) {
        self.errors.push(Box::new(error));
    }

    /// Emit all registered [`Diagnostic`]s.
    pub fn emit_errors(&self, ctx: &DuckContext) -> QuackResult<()> {
        for errors in &self.errors {
            emit_error(errors, ctx)?;
        }
        let errors_count = self.errors.len();
        if errors_count == 0 {
            return Ok(());
        }

        qp_bail!("emitted {errors_count} error{}", errors_count.s_if_plural())
    }
}

/// Emit a single [`Diagnostic`] to an appropriate [`Terminal`](crate::duck::util::terminal::Terminal).
fn emit_error(error: &impl Diagnostic, ctx: &DuckContext) -> QuackResult<()> {
    ctx.error(error)
}

macros::make_diagnostic! {
    pub struct UnusedKey {
        pub key: String,
    }
    display("the key `{}` is unused", key)
}
