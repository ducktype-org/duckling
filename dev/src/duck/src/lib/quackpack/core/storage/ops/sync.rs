use std::{path::PathBuf, time::SystemTime};

use rustvil::fs::{PathExt, ShouldBlock};
use tracing::debug;

use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId, qp_bail,
    quackpack::{
        core::{Package, PackageCtx, PackageLoader},
        schemas::registry,
    },
};

use crate::quackpack::core::storage;

const MAX_BLOC_RETRY_COUNT: usize = 3;

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

// @TODO: #1353 run

pub fn sync(ctx: &DuckCtx, pkg_ctx: &PackageCtx, options: SyncOptions) -> QuackResult<()> {
    let storage = storage::paths::StoragePaths::new(ctx.duck_home());
    let manifest = pkg_ctx.package().manifest();
    let venv_config = pkg_ctx.venv_config();
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
    let input_freeze = load_external_freezefile(pkg_ctx, expose_freezefile)?;
    let id = manifest.root_description().name();

    let _sync_lock = storage::locks::TrySyncLock::new(&storage, id)?;
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;

    let data = storage::files::fix_and_load_venv(&storage, id)?;
    let freeze: storage::files::VenvFreeze = panic!("run solver");
    debug!("solver returned freeze `{freeze:?}`");
    drop(data_lock);
    panic!("install dependencies");
    if !options.overwrite
        && let Some(data) = data
        && pkg_ctx.package().manifest_path() != data.last_location
        && data.last_location.exists()
    {
        let replaces = PackageLoader::find_at_exact_directory(&data.last_location, pkg_ctx.ctx())
            .map(|pkg| pkg.package().manifest().root_description().name() == id)
            .unwrap_or(false);
        if replaces {
            qp_bail!(
                "tried to overwrite existing virtual environment from another location. Use `--overwrite` to force an overwrite"
            )
        }
    }
    let data_lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let now = SystemTime::now();
    storage::files::save_venv(
        &storage,
        id,
        &storage::files::StorageVenv {
            freeze,
            original_schema: registry::Manifest::try_from(manifest.clone())?,
            is_ephemeral: venv_config.is_ephemeral()?,
            last_location: pkg_ctx.package().manifest_path().to_path_buf(),
            last_modification: now,
            last_access: now,
        },
    )?;
    if expose_freezefile && !options.frozen {
        let json = serde_json::to_string_pretty(&freeze)?;
        freeze_name(pkg_ctx.package()).write(json)?;
    }
    Ok(())
}

fn freeze_name(package: &Package) -> PathBuf {
    package.root_directory().join(PackageLoader::FREEZE_NAME)
}

fn load_external_freezefile(
    ctx: &PackageCtx,
    is_exposed: bool,
) -> QuackResult<Option<storage::files::VenvFreeze>> {
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
