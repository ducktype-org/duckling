//! `build` subcommand execution logic.
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit_compiler::ExecutorOutput;
use crate::quackpack::core::compile::{self, BuildContext};
use crate::quackpack::core::storage::{StorageSyncOptions, sync};
use crate::quackpack::core::{FeatureName, PackageContext};
use crate::{QuackResult, StrId};

#[derive(Debug)]
/// Options for compiling a project.
pub struct BuildOptions<'duck> {
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
}

/// Compile given options.
pub fn compile(options: BuildOptions<'_>) -> QuackResult<ExecutorOutput> {
    let BuildOptions {
        pcx,
        used_features,
        profile,
        shared,
        overwrite,
        frozen,
        strict_errors,
        jobs,
    } = options;
    let root_identity = pcx.package().as_a_local_identity()?;
    let (lock, venv, storage) = sync(
        &pcx,
        StorageSyncOptions {
            overwrite,
            frozen,
            strict_errors,
        },
    )?;
    let _compile_lock = lock.into_compile_lock();
    let profile = Profile::construct_profile(profile, pcx.package().manifest().profiles())?;
    let bcx = BuildContext {
        pcx: &pcx,
        root_identity,
        freeze: venv.into(),
        storage,
        used_features,
        profile,
        shared,
        jobs,
    };
    compile::compile(bcx)
}
