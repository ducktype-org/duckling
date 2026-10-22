// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! This module contains logic for emitting lints and warnings in QuackPack.

mod buffer;
pub mod errors;
mod passes;
pub mod rules;
pub mod warnings;

use std::fmt;
use std::str::FromStr;
use std::sync::Arc;

use buffer::LintBuffer;
use passes::{PASSES, run_single_pass};

use super::PackageContext;
use crate::{QuackError, QuackResult, qp_bail};

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

impl FromStr for LintLevel {
    type Err = QuackError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(match s {
            "allow" => Self::Allow,
            "warning" => Self::Warning,
            "error" => Self::Error,
            _ => qp_bail!(
                "invalid lint level: expected one of `allow`, `warning`, `error`, but found `{s}`"
            ),
        })
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub struct Lint {
    /// Name of the lint.
    pub name: &'static str,
    /// Description of this lint. Should explain, what the lint does, give an example, and
    /// give a reason why it's bad.
    pub description: &'static str,
    /// The default level of this lint.
    pub level: LintLevel,
}

/// A shorthand for emitting diagnostics and running lints.
pub fn emit_diagnostics_and_run_lint_passes(pcx: &PackageContext<'_>) -> QuackResult<()> {
    pcx.emit_diagnostics()?;
    run_lint_passes(pcx)
}

/// Run lints for the given package.
pub fn run_lint_passes(pcx: &PackageContext<'_>) -> QuackResult<()> {
    let mut buffer = LintBuffer::new();

    for pass in PASSES {
        run_single_pass(pcx, *pass, &mut buffer)?;
    }
    buffer.emit(pcx.ctx())
}

pub(crate) mod macros {
    /// A helper macro for declaring a struct which implements [`Diagnostic`], with the given
    /// [`Display`] implementation.
    ///
    /// # Usage
    ///
    /// This macro takes a normal struct declaration, with visibilities.
    ///
    /// Next, it takes [`Display`] impl in some special form.
    ///
    /// For example:
    /// ```rust,ignore (illustrative)
    /// make_diagnostic! {
    ///     struct Foo {},
    ///     display("")
    /// }
    /// ```
    /// generates:
    /// ```rust,ignore (illustrative)
    /// #[derive(Debug)]
    /// struct Foo {}
    ///
    /// impl ::std::fmt::Display for Foo {
    ///     fn fmt(&self, ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
    ///         write!(f, "")
    ///     }
    /// }
    ///
    /// impl Diagnostic for Foo {}
    /// ```
    ///
    /// You can also make more advanced structs and impls.
    ///
    /// ```rust,ignore (illustrative)
    /// make_diagnostic! {
    ///     pub struct Foo {
    ///         private: i32,
    ///         pub(crate) krate: String,
    ///         pub(super) foo: Vec<()>,
    ///         pub tag: bool,
    ///     },
    ///     display("private: {}, krate: {}, tag: {}", private, krate, tag)
    /// }
    /// ```
    /// generates:
    /// ```rust,ignore (illustrative)
    /// #[derive(Debug)]
    /// pub struct Foo {
    ///     private: i32,
    ///     pub(crate) krate: String,
    ///     pub(super) foo: Vec<()>,
    ///     pub tag: bool,
    /// }
    ///
    /// impl ::std::fmt::Display for Foo {
    ///     fn fmt(&self, ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
    ///         write!(f, "private: {}, krate: {}, tag: {}", self.private, self.krate, self.tag)
    ///     }
    /// }
    ///
    /// impl Diagnostic for Foo {}
    /// ```
    ///
    /// As You can see, `self.` is prepend to the arguments to [`Display`].
    ///
    /// [`Display`]: std::fmt::Display
    /// [`Diagnostic`]: crate::quackpack::core::lints::Diagnostics
    macro_rules! make_diagnostic {
    (
        $struct_vis:vis struct $name:ident {
            $(
                $field_vis:vis $field:ident: $type:ty,
            )*
        }
        display($fmt:literal $(, $arg:ident)* $(,)?)
    ) => {
        #[derive(Debug)]
        $struct_vis struct $name {
            $($field_vis $field: $type,)*
        }
        impl ::std::fmt::Display for $name {
            fn fmt(&self, f: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
                write!(f, $fmt $(, self.$arg)*)
            }
        }
        impl $crate::quackpack::core::lints::Diagnostic for $name {}
    };
}
    pub(crate) use make_diagnostic;
}
