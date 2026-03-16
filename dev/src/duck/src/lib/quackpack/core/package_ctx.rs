//! A context of a package  parsed from the disk.
use std::path::PathBuf;

use crate::{
    DuckCtx, QuackResult,
    quackpack::core::{self, Package, package_loader::PackageLoader, venv_config::VenvConfig},
};

#[derive(Debug)]
/// A context of a package  parsed from the disk.
pub struct PackageCtx<'duck> {
    package: Package,
    venv_config: VenvConfig,
    ctx: &'duck DuckCtx,
}

impl<'duck> PackageCtx<'duck> {
    /// Create new [`PackageCtx`]
    pub fn new(project_root: PathBuf, ctx: &'duck DuckCtx) -> QuackResult<Self> {
        let package = core::parse_manifest(&project_root.join(PackageLoader::MANIFEST_NAME), ctx)?;
        let venv_config_path = project_root.join(PackageLoader::VENV_CONFIG_NAME);
        let venv_config = VenvConfig::new(venv_config_path)?;
        Ok(Self {
            package,
            venv_config,
            ctx,
        })
    }

    /// Get underlying [`Package`]
    pub fn package(&self) -> &Package {
        &self.package
    }

    /// Consume self, returning the underlying package.
    pub fn into_package(self) -> Package {
        self.package
    }

    /// Get [`VenvConfig`] of this [`PackageCtx`]
    pub fn venv_config(&self) -> &VenvConfig {
        &self.venv_config
    }

    /// Get [`DuckCtx`] used to create this [`PackageCtx`]
    pub fn ctx(&self) -> &DuckCtx {
        self.ctx
    }
}

impl From<PackageCtx<'_>> for Package {
    fn from(value: PackageCtx<'_>) -> Self {
        value.package
    }
}
