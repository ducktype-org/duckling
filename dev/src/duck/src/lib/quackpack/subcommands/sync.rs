//! `sync` subcommand execution logic.
use crate::{
    DuckContext, QuackResult,
    quackpack::core::{
        AllowGlobalPackage, PackageLoader,
        storage::{self, StorageSyncOptions},
    },
};

#[derive(Debug, Default, Clone, Copy)]
/// All options that can be passed to sync.
pub struct SyncOptions {
    /// Use a global package instead of a local one.
    pub global: bool,
    /// Overwrite any existing venvs.
    pub overwrite: bool,
    /// Assume, that freezefile doesn't change.
    pub frozen: bool,
    /// Disallow any errors in foreign packages' manifests.
    pub strict_errors: bool,
}

/// Synchronize a virtual environment found from CWD, if `options.global` is false, otherwise use a
/// global environment.
pub fn sync(ctx: &DuckContext, options: SyncOptions) -> QuackResult<()> {
    let pkg = if options.global {
        PackageLoader::global_package(ctx)?
    } else {
        PackageLoader::find_from_cwd(ctx, AllowGlobalPackage::No)?
    };
    storage::sync(
        &pkg,
        StorageSyncOptions {
            overwrite: options.overwrite,
            frozen: options.frozen,
            strict_errors: options.strict_errors,
        },
    )?;
    Ok(())
}
