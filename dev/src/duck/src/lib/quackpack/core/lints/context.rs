//! Context used when running passes.

use super::{Diagnostic, Lint};
use crate::{DuckContext, QuackResult, qp_bail};

#[derive(Debug)]
struct DiagnosticWithLint {
    diagnostic: Box<dyn Diagnostic + 'static>,
    lint: Lint,
}

#[derive(Debug)]
pub struct LintContext {
    warnings: Vec<DiagnosticWithLint>,
    errors: Vec<DiagnosticWithLint>,
}

#[expect(dead_code)]
impl LintContext {
    /// Generate a new [`LintContext`], with none lints emitted.
    pub fn new() -> Self {
        Self {
            warnings: vec![],
            errors: vec![],
        }
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

        let suffix = if errors_count == 1 { "s" } else { "" };
        qp_bail!("emitted {errors_count} error{suffix}")
    }
}

fn emit_diag(diag: DiagnosticWithLint, is_error: bool, ctx: &DuckContext) -> QuackResult<()> {
    let DiagnosticWithLint { diagnostic, lint } = diag;
    let msg = format!("{diagnostic} [{}]", lint.name);
    let term = ctx.error_console();
    if is_error {
        term.error(msg)
    } else {
        term.warning(msg)
    }
}
