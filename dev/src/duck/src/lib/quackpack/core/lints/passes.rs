//! Lints passes.
//!
//! Each pass is a function which takes a [`LintContext`], and some package abstraction.

use super::context::LintContext;
use crate::quackpack::core::script::Script;
use crate::quackpack::core::{Manifest, Package, PackageContext};
use crate::{DuckContext, QuackResult};

/// A general lint, which works on any package.
pub(super) type LintFn = fn(&PackageContext<'_>, &mut LintContext) -> QuackResult<()>;

/// A  lint which fires only on packages.
pub(super) type PackageLintFn = fn(&Package, &DuckContext, &mut LintContext) -> QuackResult<()>;

/// A  lint which fires only on scripts.
pub(super) type ScriptLintFn = fn(&Script, &DuckContext, &mut LintContext) -> QuackResult<()>;

/// A  lint which always fires, but checks only manifests.
pub(super) type ManifestLintFn = fn(&Manifest, &DuckContext, &mut LintContext) -> QuackResult<()>;

#[derive(Debug, Clone, Copy)]
#[expect(dead_code)]
/// A general lint pass.
pub(super) enum LintPass {
    General(LintFn),
    Package(PackageLintFn),
    Script(ScriptLintFn),
    Manifest(ManifestLintFn),
}

/// Registered lints' passes.
pub(super) const PASSES: &[LintPass] = &[];

/// Runs a single pass on the package.
pub(super) fn run_single_pass(
    pcx: &PackageContext<'_>,
    pass: LintPass,
    context: &mut LintContext,
) -> QuackResult<()> {
    match pass {
        LintPass::General(general_fn) => general_fn(pcx, context)?,
        LintPass::Package(package_fn) => {
            if let Some(package) = pcx.package().try_get_package() {
                package_fn(package, pcx.ctx(), context)?;
            };
        }
        LintPass::Script(script_fn) => {
            if let Some(script) = pcx.package().try_get_script() {
                script_fn(script, pcx.ctx(), context)?;
            };
        }
        LintPass::Manifest(manifest_fn) => {
            manifest_fn(pcx.package().manifest(), pcx.ctx(), context)?
        }
    }
    Ok(())
}
