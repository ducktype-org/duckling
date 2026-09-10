//! Various warnings created when parsing the [`manifest`].
//!
//! They are simple diagnostics, not associated with any lint, and are not checked when running
//! [`run_lint_passes`].
//!
//! [`manifest`]: crate::quackpack::core::parse
//! [`run_lint_passes`]: super::run_lint_passes

use std::fmt;

use super::Diagnostic;
use crate::{DuckContext, QuackResult};

#[derive(Debug, Default)]
/// Warnings encountered when parsing the manifest.
pub struct Warnings {
    warnings: Vec<Box<dyn Diagnostic + 'static>>,
}

impl Warnings {
    /// Create a new [`Warnings`].
    pub fn new() -> Self {
        Self::default()
    }

    /// Register a new [`Diagnostic`] to be emitted later.
    pub fn push<T: Diagnostic + 'static>(&mut self, warning: T) {
        self.warnings.push(Box::new(warning));
    }

    /// Emit all registered [`Diagnostic`]s.
    pub fn emit_warnings(&self, ctx: &DuckContext) -> QuackResult<()> {
        for warning in &self.warnings {
            emit_warning(warning, ctx)?;
        }
        Ok(())
    }
}

/// Emit a single [`Diagnostic`] to an appropriate [`Terminal`](crate::duck::util::terminal::Terminal).
fn emit_warning(warning: &impl Diagnostic, ctx: &DuckContext) -> QuackResult<()> {
    ctx.console().warning(warning)
}

#[derive(Debug)]
/// A [`Diagnostic`] for an unused key.
pub struct UnusedKey {
    key: String,
}

impl UnusedKey {
    /// Create a new [`UnusedKey`] [`Diagnostic`] for the given key.
    pub fn new(key: String) -> Self {
        Self { key }
    }
}

impl fmt::Display for UnusedKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "the key `{}` is unused", self.key)
    }
}

impl Diagnostic for UnusedKey {}

#[derive(Debug)]
pub struct GitUrlIsPath {
    url: String,
    path_to_dep: String,
}

impl GitUrlIsPath {
    pub fn new(url: String, path_to_dependency: String) -> Self {
        Self {
            url,
            path_to_dep: path_to_dependency,
        }
    }
}

impl fmt::Display for GitUrlIsPath {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "git url `{}` of a dependency `{}` is a path; it can cause surprising effects",
            self.url, self.path_to_dep
        )
    }
}

impl Diagnostic for GitUrlIsPath {}
