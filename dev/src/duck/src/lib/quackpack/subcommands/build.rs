//! `build` subcommand execution logic.
use crate::{
    QuackResult, QuackResultContext, StrId,
    quackpack::core::{
        FeatureName, PackageCtx,
        compile::{self, BuildContext, profiles::Profile},
        storage::{StorageSyncOptions, sync, venv_id::ToVenvId},
    },
};

#[derive(Debug)]
/// Options for compiling a project.
pub struct BuildOptions<'duck> {
    /// Package to compile.
    pub package: PackageCtx<'duck>,
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
}

/// Compile given options.
pub fn compile(options: BuildOptions<'_>) -> QuackResult<()> {
    let BuildOptions {
        package,
        used_features,
        profile,
        overwrite,
        frozen,
        strict_errors,
    } = options;
    let (lock, venv, storage) = sync(
        &package,
        StorageSyncOptions {
            overwrite,
            frozen,
            strict_errors,
        },
    )?;
    let _compile_lock = lock
        .to_compile_lock(&storage, package.to_venv_id())
        .context("failed to acquire a compile lock")?;
    let profile = Profile::construct_profile(profile, package.package().manifest().profiles())?;
    let bcx = BuildContext {
        package: &package,
        freeze: venv.into(),
        storage,
        used_features,
        profile,
    };
    compile::compile(bcx)?;
    Ok(())
}
