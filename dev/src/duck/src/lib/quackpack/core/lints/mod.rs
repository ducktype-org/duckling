//! This module contains logic for emitting lints and warnings in QuackPack.

mod buffer;
mod passes;
pub mod warnings;

use std::fmt;
use std::sync::Arc;

use buffer::LintBuffer;
use passes::{PASSES, run_single_pass};

use super::PackageContext;
use crate::QuackResult;

/// A general diagnostic which can be emitted to a user.
pub trait Diagnostic: fmt::Debug + fmt::Display + Sync + Send {}

impl<T: Diagnostic + ?Sized> Diagnostic for &T {}
impl<T: Diagnostic + ?Sized> Diagnostic for &mut T {}
impl<T: Diagnostic + ?Sized> Diagnostic for Box<T> {}
impl<T: Diagnostic + ?Sized> Diagnostic for Arc<T> {}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// A level of a lint.
pub enum LintLevel {
    /// This lint can be allowed, and therefore ignored in printing.
    Allow,
    /// This level is a warning. It will not cause [`LintBuffer::emit`] to bail.
    Warning,
    /// This level is an error. It will cause [`LintBuffer::emit`] to fail at the end.
    Error,
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub struct Lint {
    /// Name of the lint.
    pub name: &'static str,
    /// Description of this lint. Should explain, what the lint does, give an example, and
    /// give a reason why it's bad.
    pub description: &'static str,
}

/// A shorthand for emitting warnings and lints.
pub fn emit_warnings_and_run_lint_passes(pcx: &PackageContext<'_>) -> QuackResult<()> {
    pcx.emit_warnings()?;
    run_lint_passes(pcx)
}

/// Emit lints for the given package.
pub fn run_lint_passes(pcx: &PackageContext<'_>) -> QuackResult<()> {
    let mut lint_context = LintBuffer::new();

    for pass in PASSES {
        run_single_pass(pcx, *pass, &mut lint_context)?;
    }
    lint_context.emit(pcx.ctx())
}
