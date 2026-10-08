// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Lints passes.
//!
//! Each pass is a function which takes a [`LintBuffer`], and some package abstraction.
//!
//! The goal of each function is to check whether some bad pattern occurs, and register that
//! information in [`LintBuffer`].

use super::buffer::LintBuffer;
use super::rules;
use crate::quackpack::core::script::Script;
use crate::quackpack::core::{Manifest, Package, PackageContext};
use crate::{DuckContext, QuackResult};

/// A general lint, which works on both packages and scripts.
pub(super) type LintFn = fn(&PackageContext<'_>, &mut LintBuffer) -> QuackResult<()>;

/// A  lint which fires only on packages.
pub(super) type PackageLintFn = fn(&Package, &DuckContext, &mut LintBuffer) -> QuackResult<()>;

/// A  lint which fires only on scripts.
pub(super) type ScriptLintFn = fn(&Script, &DuckContext, &mut LintBuffer) -> QuackResult<()>;

/// A  lint which always fires, but checks only manifests.
pub(super) type ManifestLintFn = fn(&Manifest, &DuckContext, &mut LintBuffer) -> QuackResult<()>;

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
pub(super) const PASSES: &[LintPass] = &[
    LintPass::Manifest(rules::always_false_conditions::pass),
    LintPass::Manifest(rules::nonexistent_features::pass),
    LintPass::Manifest(rules::self_implying_features::pass),
    LintPass::General(rules::aliases_equal_to_names::pass),
];

/// Runs a single pass on the package.
pub(super) fn run_single_pass(
    pcx: &PackageContext<'_>,
    pass: LintPass,
    buffer: &mut LintBuffer,
) -> QuackResult<()> {
    match pass {
        LintPass::General(general_fn) => general_fn(pcx, buffer)?,
        LintPass::Package(package_fn) => {
            if let Some(package) = pcx.package().try_get_package() {
                package_fn(package, pcx.ctx(), buffer)?;
            };
        }
        LintPass::Script(script_fn) => {
            if let Some(script) = pcx.package().try_get_script() {
                script_fn(script, pcx.ctx(), buffer)?;
            };
        }
        LintPass::Manifest(manifest_fn) => {
            manifest_fn(pcx.package().manifest(), pcx.ctx(), buffer)?
        }
    }
    Ok(())
}
