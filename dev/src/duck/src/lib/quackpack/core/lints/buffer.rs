// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Buffer for emitted lints.

use super::{Diagnostic, Lint, LintLevel};
use crate::util::Pluralize;
use crate::{DuckContext, QuackResult, qp_bail, qp_bail_internal};

#[derive(Debug)]
struct DiagnosticWithLint {
    diagnostic: Box<dyn Diagnostic + 'static>,
    lint: Lint,
}

#[derive(Debug, Default)]
/// Buffer for registered [`Lint`]s with [`Diagnostic`]s.
///
/// Whenever [`register_warning`] or [`register_error`] is called, the message isn't emitted, but
/// stored in a buffer. This allows emitting all diagnostics at once.
///
/// To emit them, [`emit`] must be called.
///
/// [`register_warning`]: LintBuffer::register_warning
/// [`register_error`]: LintBuffer::register_error
/// [`emit`]: LintBuffer::emit
pub struct LintBuffer {
    warnings: Vec<DiagnosticWithLint>,
    errors: Vec<DiagnosticWithLint>,
}

impl LintBuffer {
    /// Generate a new [`LintBuffer`], with none lints emitted.
    pub fn new() -> Self {
        Self::default()
    }

    /// Register a new warning to be emitted later.
    pub fn register_warning<T: Diagnostic + 'static>(&mut self, diag: T, lint: Lint) {
        self.warnings.push(DiagnosticWithLint {
            diagnostic: Box::new(diag),
            lint,
        });
    }

    /// Register a new error to be emitted later.
    pub fn register_error<T: Diagnostic + 'static>(&mut self, diag: T, lint: Lint) {
        self.errors.push(DiagnosticWithLint {
            diagnostic: Box::new(diag),
            lint,
        });
    }

    /// Register a new lint with the given level to be emitted later.
    pub fn register<T: Diagnostic + 'static>(
        &mut self,
        diag: T,
        lint: Lint,
        level: LintLevel,
    ) -> QuackResult<()> {
        match level {
            LintLevel::Warning => self.register_warning(diag, lint),
            LintLevel::Error => self.register_error(diag, lint),
            LintLevel::Allow => {
                qp_bail_internal!("attempted to register lint with level {level:?}")
            }
        }
        Ok(())
    }

    /// Emit all collected warnings and errors.
    pub fn emit(self, ctx: &DuckContext) -> QuackResult<()> {
        let errors_count = self.errors.len();
        for warning in self.warnings {
            emit_diag(warning, /* is_error */ false, ctx)?;
        }

        for error in self.errors {
            emit_diag(error, /* is_error */ true, ctx)?;
        }

        if errors_count == 0 {
            return Ok(());
        }

        qp_bail!("emitted {errors_count} error{}", errors_count.s_if_plural())
    }
}

/// Emit a single [`DiagnosticWithLint`] to an appropriate
/// [`Terminal`](crate::duck::util::terminal::Terminal).
///
/// If `is_error` is true, then the lint is treated as an error. This will use `.error()` call.
/// Otherwise it's treated as a warning.
///
/// Helper for [`emit`](LintBuffer::emit).
fn emit_diag(diag: DiagnosticWithLint, is_error: bool, ctx: &DuckContext) -> QuackResult<()> {
    let DiagnosticWithLint { diagnostic, lint } = diag;
    let msg = format!("{diagnostic} [{}]", lint.name);
    if is_error {
        ctx.error(msg)
    } else {
        ctx.warning(msg)
    }
}
