#![allow(unreachable_code)] // @TODO: #1962 Remove this
use std::{path::PathBuf, time::SystemTime};

use rustvil::fs::{PathExt, ShouldBlock};
use tracing::debug;

use crate::{
    DuckCtx, QuackResult, QuackResultContext, qp_err,
    quackpack::core::{
        Package, PackageCtx, PackageLoader,
        storage::{
            locks::CompileLock,
            paths::Storage,
            venv::{Venv, VenvData},
            venv_id::ToVenvId,
        },
    },
};

use crate::quackpack::core::storage;

#[derive(Debug, Default, Clone, Copy)]
pub struct SyncOptions {
    pub overwrite: bool,
    pub frozen: bool,
    pub offline: bool,
}

/// Synchronize virtual environment for package, and return information required to build it.
///
/// Why is it safe:
///
/// Even though we drop `SyncLock`, we block cleanups from happening. And because solver only
/// returns *new* packages to install, we never remove nor overwrite anything in [`sync`],
/// therefore we can drop `SyncLock`.
pub fn sync(
    ctx: &DuckCtx,
    pkg_ctx: &PackageCtx,
    _options: SyncOptions,
) -> QuackResult<(CompileLock, Venv, Storage)> {
    let storage = Storage::new(ctx.duck_home());
    let venv_config = pkg_ctx.venv_config();
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
    let user_exposed_freeze = load_external_freezefile(pkg_ctx, expose_freezefile)?;
    let id = pkg_ctx.to_venv_id();

    let _sync_lock = storage::locks::TrySyncLock::new(&storage, id)?;
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;

    let venv = Venv::fix_and_load(&storage, id)?;
    drop(data_lock);
    let _input_freeze = user_exposed_freeze.as_ref().or_else(|| {
        if expose_freezefile {
            venv.as_ref().map(|venv| venv.data().freeze())
        } else {
            None
        }
    });
    #[allow(clippy::diverging_sub_expression)] // @TODO: #1962 Remove this
    let _freeze: storage::freeze::VenvFreeze = panic!("@TODO: #1962 Unmock solver");
    debug!("solver returned freeze `{_freeze:?}`");
    panic!("@TODO: #1962 download dependencies");
    if !_options.overwrite
        && let Some(venv) = venv
        && pkg_ctx.package().manifest_path() != venv.data().last_location()
        && venv.data().last_location().exists()
    {
        let replaces =
            PackageLoader::find_at_exact_directory(venv.data().last_location(), pkg_ctx.ctx())
                .map(|pkg| pkg.package().manifest().root_description().name() == id)
                .unwrap_or(false);
        if replaces {
            return Err(qp_err!(
                "tried to overwrite an existing virtual environment from another location"
            )
            .add_hint("use `--overwrite` to force an overwrite"));
        }
    }
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let now = SystemTime::now();
    // @TODO: #1962 Skip this, if we have nothing to install.
    // Maybe we should bump `access_time` only in that case?
    let data = VenvData::new(
        _freeze,
        venv_config.is_ephemeral()?,
        pkg_ctx.package().manifest_path().to_path_buf(),
        now,
    );
    let mut venv = Venv::new(id, data);
    venv.save_to(&storage)?;
    drop(data_lock);
    if expose_freezefile && !_options.frozen {
        let json = serde_json::to_string_pretty(&_freeze)?;
        freeze_name(pkg_ctx.package()).write(json)?;
    }
    Ok((_sync_lock.to_compile_lock(&storage, id)?, venv, storage))
}

fn freeze_name(package: &Package) -> PathBuf {
    package.root_directory().join(PackageLoader::FREEZE_NAME)
}

fn load_external_freezefile(
    ctx: &PackageCtx,
    is_exposed: bool,
) -> QuackResult<Option<storage::freeze::VenvFreeze>> {
    if !is_exposed {
        return Ok(None);
    }
    let freeze_path = freeze_name(ctx.package());
    if !freeze_path.is_file() {
        return Ok(None);
    }
    let content = freeze_path
        .read_to_string()
        .with_context(|| format!("while reading freezefile `{}`", freeze_path.display()))?;

    serde_json::from_str(&content)
        .with_context(|| format!("malformed freezefile `{}`", freeze_path.display()))
        .map(Some)
}
