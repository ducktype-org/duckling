//! This module contains logic for emitting lints and warnings in QuackPack.

pub mod warnings;

use std::fmt;
use std::sync::Arc;

use super::script::Script;
use super::{Manifest, Package, PackageContext};
use crate::{DuckContext, QuackResult, qp_bail};

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// A level of a lint.
pub enum LintLevel {
    /// This lint can be allowed, and therefore ignored in printing.
    Allow,
    /// This level is a warning. It will be reported to a user, but passes will still continue.
    Warning,
    /// This level is an error. It shouldn't be ignored.
    Error,
}

impl LintLevel {
    /// Returns `true` if the lint level is [`Warning`].
    ///
    /// [`Warning`]: LintLevel::Warning
    #[must_use]
    pub fn is_warning(self) -> bool {
        matches!(self, Self::Warning)
    }

    /// Returns `true` if the lint level is [`Error`].
    ///
    /// [`Error`]: LintLevel::Error
    #[must_use]
    pub fn is_error(self) -> bool {
        matches!(self, Self::Error)
    }
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
/// Behaviour of [`LintContext::emit`] for different [`LintLevel`]s.
pub enum EmitLintBehaviour {
    /// Even if there were some [`LintLevel::Error`]s, we don't bail at the end.
    NoBailForErrors,
    /// The “default” behaviour. No special treatment.
    Default,
    /// Every [`LintLevel::Warning`] is treated as [`LintLevel::Error`].
    TreatWarningsAsErrors,
}

#[derive(Debug)]
pub struct LintContext {
    registered_lints: Vec<Box<dyn Lint + 'static>>,

    behaviour: EmitLintBehaviour,
}

impl LintContext {
    /// Generate a new [`LintContext`], with none emitted lints.
    pub fn new(behaviour: EmitLintBehaviour) -> Self {
        Self {
            registered_lints: vec![],
            behaviour,
        }
    }

    /// Register a new [`Lint`] to be emitted later.
    pub fn register<T: Lint + 'static>(&mut self, lint: T) {
        self.registered_lints.push(Box::new(lint));
    }

    /// Get the registered lints by this context.
    pub fn registered_lints(&self) -> &[Box<dyn Lint + 'static>] {
        &self.registered_lints
    }

    /// Get the number of registered lints.
    pub fn registered_lints_count(&self) -> usize {
        self.registered_lints.len()
    }

    /// Returns true if this context has registered any lint.
    pub fn has_registered_lints(&self) -> bool {
        self.registered_lints_count() > 0
    }

    /// Get the [`EmitLintBehaviour`] for this context.
    pub fn behaviour(&self) -> EmitLintBehaviour {
        self.behaviour
    }

    /// Emit all registered lints, and return basing on [`EmitLintBehaviour`].
    pub fn emit(self, ctx: &DuckContext) -> QuackResult<()> {
        if !self.has_registered_lints() {
            return Ok(());
        }
        let mut errors = 0usize;
        for lint in self.registered_lints {
            match lint.level() {
                LintLevel::Allow => {}
                // @TODO: #3318 Use if let guards <https://github.com/rust-lang/rfcs/pull/2294> from
                // Rust 1.95. So this code can become:
                // ```rust
                // LintLevel::Warning if self.behaviour != EmitLintBehaviour::TreatWarningsAsErrors
                // => {},
                // LintLevel::Warning | LintLevel::Error => {},
                // ```
                LintLevel::Warning => {
                    if self.behaviour == EmitLintBehaviour::TreatWarningsAsErrors {
                        errors += 1;
                        ctx.console().error(lint)?;
                    } else {
                        ctx.console().warning(lint)?
                    }
                }
                LintLevel::Error => {
                    errors += 1;
                    ctx.console().error(lint)?;
                }
            };
        }
        if errors == 0 || self.behaviour == EmitLintBehaviour::NoBailForErrors {
            return Ok(());
        };
        let suffix = if errors == 1 { "" } else { "s" };
        qp_bail!("stopping because emitted {errors} error lint{suffix}")
    }
}

pub trait Lint: fmt::Debug + fmt::Display + Send + Sync {
    /// Get the [`LintLevel`] of this lint.
    fn level(&self) -> LintLevel;
}

impl<T: Lint + ?Sized> Lint for &T {
    fn level(&self) -> LintLevel {
        (*self).level()
    }
}

impl<T: Lint + ?Sized> Lint for &mut T {
    fn level(&self) -> LintLevel {
        (**self).level()
    }
}

impl<T: Lint + ?Sized> Lint for Box<T> {
    fn level(&self) -> LintLevel {
        (**self).level()
    }
}

impl<T: Lint + ?Sized> Lint for Arc<T> {
    fn level(&self) -> LintLevel {
        (**self).level()
    }
}

/// A general lint, which works on any package.
type GeneralLint = fn(&PackageContext<'_>, &mut LintContext) -> QuackResult<()>;
/// A  lint which fires only on packages.
type PackageLint = fn(&Package, &DuckContext, &mut LintContext) -> QuackResult<()>;
/// A  lint which fires only on scripts.
type ScriptLint = fn(&Script, &DuckContext, &mut LintContext) -> QuackResult<()>;
/// A  lint which always fires, but checks only manifests.
type ManifestLint = fn(&Manifest, &DuckContext, &mut LintContext) -> QuackResult<()>;

#[derive(Debug, Clone, Copy)]
#[expect(dead_code)]
/// A general lint.
enum LintFn {
    General(GeneralLint),
    Package(PackageLint),
    Script(ScriptLint),
    Manifest(ManifestLint),
}

/// Registered lints' passes.
const LINTS: &[LintFn] = &[];

/// A shorthand for emitting warnings and lints.
pub fn emit_lints_and_warnings(
    pcx: &PackageContext<'_>,
    behaviour: EmitLintBehaviour,
) -> QuackResult<()> {
    pcx.emit_warnings()?;
    emit_lints(pcx, behaviour)
}

/// Emit lints for the given package.
pub fn emit_lints(pcx: &PackageContext<'_>, behaviour: EmitLintBehaviour) -> QuackResult<()> {
    let mut lint_context = LintContext::new(behaviour);

    for lint in LINTS {
        run_single_lint(pcx, *lint, &mut lint_context)?;
    }
    lint_context.emit(pcx.ctx())
}

/// Runs a single pass on the package.
fn run_single_lint(
    pcx: &PackageContext<'_>,
    lint: LintFn,
    context: &mut LintContext,
) -> QuackResult<()> {
    match lint {
        LintFn::General(general_fn) => general_fn(pcx, context)?,
        LintFn::Package(package_fn) => {
            if let Some(package) = pcx.package().try_get_package() {
                package_fn(package, pcx.ctx(), context)?;
            };
        }
        LintFn::Script(script_fn) => {
            if let Some(script) = pcx.package().try_get_script() {
                script_fn(script, pcx.ctx(), context)?;
            };
        }
        LintFn::Manifest(manifest_fn) => manifest_fn(pcx.package().manifest(), pcx.ctx(), context)?,
    }
    Ok(())
}
