//! `build` subcommand execution logic.
use crate::quackpack::core::compile::duckc::CompilationType;
use crate::quackpack::core::compile::profiles::Profile;
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
        pcx,
        used_features,
        profile,
        overwrite,
        frozen,
        strict_errors,
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
        script_path: None,
    };
    compile::compile(bcx, CompilationType::OnlyRootPackage)?;
    Ok(())
}
