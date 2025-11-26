use std::{marker::PhantomData, path::Path};

use anyhow::{Context, bail};
use rustvil::fs::PathExt;
use tracing::{debug, trace};

#[cfg(test)]
mod tests;

use crate::{
    QpCtx, QuackResult,
    quackpack::{core::PackageCtx, util::paths::MANIFEST_FILENAME},
};

#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum AllowGlobalPackage {
    No,
    Yes,
}

impl From<bool> for AllowGlobalPackage {
    fn from(value: bool) -> Self {
        match value {
            true => AllowGlobalPackage::Yes,
            false => AllowGlobalPackage::No,
        }
    }
}

impl AllowGlobalPackage {
    pub fn allows(&self) -> bool {
        matches!(self, AllowGlobalPackage::Yes)
    }
}

// Disallow creating PackageLoader instances.
pub struct PackageLoader(PhantomData<()>);

impl PackageLoader {
    pub const MANIFEST_NAME: &str = "quackconfig.yml";
    pub const FREEZE_NAME: &str = "quackfreeze.json";
    pub const VENV_CONFIG_NAME: &str = "venvconfig.toml";
    pub const LOCAL_STORAGE_NAME: &str = ".storage";

    pub fn global_package<'duck>(_ctx: QpCtx<'duck>) -> QuackResult<PackageCtx<'duck>> {
        bail!("@TODO: #1572 it uses EditableManifest")
    }

    pub fn find_from_directory<'duck>(
        start: &Path,
        ctx: QpCtx<'duck>,
        allow_global_package: AllowGlobalPackage,
    ) -> QuackResult<PackageCtx<'duck>> {
        let start = start
            .expand_user()
            .with_context(|| {
                format!(
                    "failed to expand the tildes of the path `{}`",
                    start.display()
                )
            })?
            .resolve()
            .with_context(|| {
                format!(
                    "failed to resolve the symlinks of the path `{}`",
                    start.display()
                )
            })?;
        if !start.is_dir() {
            bail!("the path `{}` is not a directory", start.display())
        }
        let mut current: &Path = start.as_ref();
        for potential_location in start.ancestors() {
            let path = potential_location.join(MANIFEST_FILENAME);
            trace!("checking the path `{}`", path.display());
            if path.is_file() {
                debug!("found a package at `{}`", path.display());
                return PackageCtx::new(potential_location.to_path_buf(), ctx);
            }
            current = potential_location;
        }
        if allow_global_package.allows() {
            PackageLoader::global_package(ctx)
        } else {
            bail!(
                "no manifest has been found from the `{}` to the `{}`",
                start.display(),
                current.display()
            )
        }
    }

    pub fn find_at_exact_directory<'duck>(
        path: &Path,
        ctx: QpCtx<'duck>,
    ) -> QuackResult<PackageCtx<'duck>> {
        if !path.is_dir() {
            bail!("the path `{}` is not a directory", path.display())
        }
        let manifest_path = path.join(PackageLoader::MANIFEST_NAME);
        if !manifest_path.is_file() {
            bail!("the directory `{}` has no manifest", path.display())
        }
        PackageCtx::new(path.to_path_buf(), ctx)
    }
}
