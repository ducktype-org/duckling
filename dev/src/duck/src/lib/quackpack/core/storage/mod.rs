//! Provides high level storage operations. Access control is provided by [`locks`]
//! module. Operations for state modification, which preserve coherency, are
//! provided by [`files`] module.

use std::{collections::HashSet, path::PathBuf};

use rustvil::fs::{PathExt, ShouldBlock};
use tracing::debug;

use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId,
    quackpack::core::{Manifest, Package, PackageCtx, PackageLoader, Source},
};

mod clean;
mod files;
mod git_access;
mod locks;
mod miscellaneous;
mod package_id;
mod paths;

pub use clean::*;
pub use miscellaneous::*;

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

pub fn sync_venv(ctx: &DuckCtx, pkg_ctx: &PackageCtx, options: SyncOptions) -> QuackResult<()> {
    let storage = paths::StoragePaths::new(ctx.duck_home());
    let manifest = pkg_ctx.package().manifest();
    let venv_config = pkg_ctx.venv_config();
    let expose_freezefile = venv_config.is_freezefile_exposed()?;
    let input_freeze = load_external_freezefile(pkg_ctx)?;
    let id = manifest.root_description().name();
    let _sync_lock = locks::TrySyncLock::new(&storage, id)?;
    let _lock = storage.data_lock(id).lock(ShouldBlock::Yes)?;
    let data = files::fix_and_load_venv(&storage, id)?;
    if is_synchronized(
        Some(manifest),
        input_freeze.as_ref(),
        data.as_ref(),
        expose_freezefile,
    ) {
        debug!("synchronized");
        return Ok(());
    }
    todo!()
}

fn freeze_name(package: &Package) -> PathBuf {
    package.root_directory().join(PackageLoader::FREEZE_NAME)
}

fn load_external_freezefile(ctx: &PackageCtx) -> QuackResult<Option<files::VenvFreeze>> {
    if !ctx.venv_config().is_freezefile_exposed()? {
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

fn check_direct_dependencies(manifest: &Manifest, freeze: &files::VenvFreeze) -> bool {
    if manifest.dependencies().all_dependencies().len() != freeze.direct_dependencies.len() {
        debug!("length mismatch");
        return false;
    }
    for (alias, dep) in manifest.dependencies().all_dependencies() {
        let Some(freeze_dep_id) = freeze.direct_dependencies.get(alias) else {
            debug!("freeze doesn't have direct dependency `{alias}`");
            return false;
        };
        debug!("checking `{dep:#?}` against `{freeze_dep_id:#?}`");
        let forced_features = dep
            .enabled_features(manifest.features().all_features().keys().copied().collect())
            .into_iter()
            .collect::<HashSet<_>>();
        if !forced_features.is_subset(
            &freeze
                .dependencies
                .get(freeze_dep_id)
                .expect("corrupted storage")
                .used_flags
                .iter()
                .copied()
                .collect(),
        ) {
            debug!("missing features, forced are `{forced_features:?}`");
            return false;
        }
        if dep.is_pinned() {
            let package_id::PackageId::Registry(freeze_registry) = freeze_dep_id else {
                return false;
            };
            if dep.real_name() != freeze_registry.id {
                return false;
            }
            let Source::Registry(ref registry) = *dep.desc().source() else {
                return false;
            };
            if freeze_registry.url.as_str() != registry.url().as_str() {
                return false;
            }
            if dep.desc().versions()[0] != freeze_registry.version {
                return false;
            }
            continue;
        }
        match (dep.desc().source().as_ref(), freeze_dep_id) {
            (Source::Registry(registry), package_id::PackageId::Registry(registry_id)) => {
                if dep.real_name() != registry_id.id {
                    return false;
                }
                if registry.url().as_str() != registry_id.url.as_str() {
                    return false;
                }
                if !dep
                    .desc()
                    .versions()
                    .iter()
                    .any(|ver| ver.can_be_upgraded_to(&registry_id.version))
                {
                    debug!("version check failed");
                    return false;
                }
            }
            (Source::Local(local), package_id::PackageId::Local(local_id)) => {
                if local.absolute() != local_id.path {
                    return false;
                }
            }
            (Source::Git(git), package_id::PackageId::Git(git_id)) => {
                if git.url().as_str() != git_id.url.as_str() {
                    return false;
                }
            }
            _ => return false,
        }
    }
    true
}

fn is_synchronized(
    manifest: Option<&Manifest>,
    user_freeze: Option<&files::VenvFreeze>,
    venv: Option<&files::StorageVenv>,
    freezefile_exposed: bool,
) -> bool {
    let Some(venv) = venv else {
        debug!("missing StorageVenv");
        return false;
    };
    if (user_freeze.is_some() || freezefile_exposed) && user_freeze != Some(&venv.freeze) {
        debug!("freezefile mismatch");
        return false;
    }
    if let Some(manifest) = manifest
        && !check_direct_dependencies(manifest, &venv.freeze)
    {
        debug!("dependencies not satisfied");
        return false;
    }
    true
}
