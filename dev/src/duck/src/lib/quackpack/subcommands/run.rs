//! `build` subcommand execution logic.
use std::ffi::OsString;

use crate::quackpack::core::{FeatureName, PackageContext, run};
use crate::quackpack::subcommands::build::{self, BuildOptions};
use crate::{QuackResult, StrId};

#[derive(Debug)]
/// Options for compiling a project.
pub struct RunOptions<'duck> {
    /// Package to compile.
    pub pcx: PackageContext<'duck>,
    /// Enabled features from the CLI.
    pub used_features: Vec<FeatureName>,
    /// Selected build profile.
    pub profile: StrId,
    /// Artefact from [`StorageSyncOptions`].
    pub overwrite: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub frozen: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub strict_errors: bool,
    /// Number of threads to use.
    pub jobs: usize,
    /// Arguments to the binary.
    pub args: Vec<OsString>,
}

impl<'a> From<RunOptions<'a>> for (BuildOptions<'a>, Vec<OsString>) {
    fn from(val: RunOptions<'a>) -> Self {
        let build_opts = BuildOptions {
            pcx: val.pcx,
            used_features: val.used_features,
            profile: val.profile,
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
pub fn run(options: RunOptions<'_>) -> QuackResult<()> {
    let (build_options, args) = options.into();
    let output = build::compile(build_options)?;
    run::run(output, args)
}
