#![allow(unreachable_code)] // @TODO: #1962 Remove this
use std::{path::PathBuf, time::SystemTime};

use rustvil::fs::{PathExt, ShouldBlock};
use tracing::debug;

use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId, qp_bail,
    quackpack::{
        core::{
            Package, PackageCtx, PackageLoader,
            storage::{locks::DisallowCleanLock, paths::StoragePaths, venv::Venv},
        },
        schemas::registry,
    },
};

use crate::quackpack::core::storage;

#[derive(Debug)]
pub enum IdOrPackage<'a> {
    Id(StrId),
    Package(&'a PackageCtx<'a>),
}

impl IdOrPackage<'_> {
    pub fn venv_id(&self) -> StrId {
        match self {
            Self::Id(id) => *id,
            Self::Package(ctx) => ctx.package().manifest().root_description().name(),
        }
    }
}

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
) -> QuackResult<(DisallowCleanLock, Venv, StoragePaths)> {
    let storage = StoragePaths::new(ctx.duck_home());
    let manifest = pkg_ctx.package().manifest();
    let venv_config = pkg_ctx.venv_config();
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
    let _input_freeze = load_external_freezefile(pkg_ctx, expose_freezefile)?;
    let id = manifest.root_description().name();

    let _sync_lock = storage::locks::TrySyncLock::new(&storage, id)?;
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;

    let _data = storage::venv::fix_and_load_venv(&storage, id)?;
    drop(data_lock);
    #[allow(clippy::diverging_sub_expression)] // @TODO: #1962 Remove this
    let _freeze: storage::freeze::VenvFreeze = panic!("@TODO: #1962 Unmock solver");
    debug!("solver returned freeze `{_freeze:?}`");
    panic!("@TODO: #1962 download dependencies");
    if !_options.overwrite
        && let Some(data) = _data
        && pkg_ctx.package().manifest_path() != data.last_location()
        && data.last_location().exists()
    {
        let replaces = PackageLoader::find_at_exact_directory(data.last_location(), pkg_ctx.ctx())
            .map(|pkg| pkg.package().manifest().root_description().name() == id)
            .unwrap_or(false);
        if replaces {
            qp_bail!(
                "tried to overwrite an existing virtual environment from another location. Use `--overwrite` to force an overwrite"
            )
        }
    }
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let now = SystemTime::now();
    // @TODO: #1962 Skip this, if we have nothing to install.
    // Maybe we should bump `access_time` only in that case?
    let venv = Venv::new(
        _freeze,
        registry::Manifest::try_from(manifest.clone())?,
        venv_config.is_ephemeral()?,
        pkg_ctx.package().manifest_path().to_path_buf(),
        now,
        now,
    );
    storage::venv::save_venv(&storage, id, &venv)?;
    drop(data_lock);
    if expose_freezefile && !_options.frozen {
        let json = serde_json::to_string_pretty(&_freeze)?;
        freeze_name(pkg_ctx.package()).write(json)?;
    }
    Ok((_sync_lock.to_disallow_clean_lock(), venv, storage))
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
