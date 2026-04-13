//! A context of a package  parsed from the disk.
use std::path::PathBuf;

use crate::{
    DuckContext, QuackResult,
    quackpack::core::{self, Package, package_loader::PackageLoader, venv_config::VenvConfig},
};

#[derive(Debug)]
/// A context of a package  parsed from the disk.
pub struct PackageContext<'duck> {
    package: Package,
    venv_config: VenvConfig,
    ctx: &'duck DuckContext,
}

impl<'duck> PackageContext<'duck> {
    /// Create new [`PackageCtx`]
    #[tracing::instrument(skip_all)]
    pub fn new(project_root: PathBuf, ctx: &'duck DuckContext) -> QuackResult<Self> {
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
    pub fn ctx(&self) -> &DuckContext {
        self.ctx
    }
}

impl From<PackageContext<'_>> for Package {
    fn from(value: PackageContext<'_>) -> Self {
        value.package
    }
}
