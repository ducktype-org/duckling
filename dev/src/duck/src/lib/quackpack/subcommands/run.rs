//! `build` subcommand execution logic.
use std::convert::Infallible;
use std::ffi::OsStr;

use crate::quackpack::core::{FeatureName, PackageContext, run};
use crate::quackpack::subcommands::build::{self, BuildOptions};
use crate::{QuackResult, StrId};

#[derive(Debug)]
/// Options for compiling a project.
pub struct RunOptions<'duck, 'matches> {
    /// Package to compile.
    pub pcx: PackageContext<'duck>,
    /// Enabled features from the CLI.
    pub used_features: Vec<FeatureName>,
    /// Selected build profile.
    pub profile: StrId,
    /// Whether to compile all dependencies into single folder (`false`) or compile each one where its code is located (`true`).
    pub shared: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub overwrite: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub frozen: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub strict_errors: bool,
    /// Number of threads to use.
    pub jobs: usize,
    /// Arguments to the binary.
    pub args: Vec<&'matches OsStr>,
}

impl<'a, 'b> From<RunOptions<'a, 'b>> for (BuildOptions<'a>, Vec<&'b OsStr>) {
    fn from(val: RunOptions<'a, 'b>) -> Self {
        let build_opts = BuildOptions {
            pcx: val.pcx,
            used_features: val.used_features,
            profile: val.profile,
            shared: val.shared,
            overwrite: val.overwrite,
            frozen: val.frozen,
            strict_errors: val.strict_errors,
            jobs: val.jobs,
        };
        let args = val.args;
        (build_opts, args)
    }
}

/// Compile and run given options.
pub fn run(options: RunOptions<'_, '_>) -> QuackResult<Infallible> {
    let (build_options, args) = options.into();
    let output = build::compile(build_options)?;
    run::run(output, args)
}
