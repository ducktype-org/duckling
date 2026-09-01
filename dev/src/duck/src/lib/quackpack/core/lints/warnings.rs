//! Various warnings created when parsing the [`manifest`].
//!
//! [`manifest`]: crate::quackpack::core::parse

use std::fmt;
use std::sync::Arc;

use crate::{DuckContext, QuackResult};

pub trait Warning: fmt::Debug + fmt::Display + Sync + Send {}

impl<T: Warning + ?Sized> Warning for &T {}
impl<T: Warning + ?Sized> Warning for &mut T {}
impl<T: Warning + ?Sized> Warning for Box<T> {}
impl<T: Warning + ?Sized> Warning for Arc<T> {}

#[derive(Debug, Default, Clone)]
/// Warnings encountered when parsing the manifest.
pub struct Warnings {
    warnings: Vec<Arc<dyn Warning + 'static>>,
}

impl Warnings {
    /// Create a new [`Warnings`].
    pub fn new() -> Self {
        Self::default()
    }

    /// Register a new [`Warning`] to be emitted later.
    pub fn push<T: Warning + 'static>(&mut self, warning: T) {
        self.warnings.push(Arc::new(warning));
    }

    /// Emit all registered [`Warning`]s.
    pub fn emit_warnings(&self, ctx: &DuckContext) -> QuackResult<()> {
        for warning in &self.warnings {
            emit_warning(warning, ctx)?;
        }
        Ok(())
    }
}

/// Emit a single [`Warning`] to an appropriate [`Terminal`](crate::duck::util::terminal::Terminal).
fn emit_warning(warning: &impl Warning, ctx: &DuckContext) -> QuackResult<()> {
    ctx.console().warning(warning)
}

#[derive(Debug)]
/// A [`Warning`] for an unused key.
pub struct UnusedKey {
    key: String,
}

impl UnusedKey {
    /// Create a new [`UnusedKey`] [`Warning`] for the given key.
    pub fn new(key: String) -> Self {
        Self { key }
    }
}

impl fmt::Display for UnusedKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "the key `{}` is unused", self.key)
    }
}

impl Warning for UnusedKey {}
