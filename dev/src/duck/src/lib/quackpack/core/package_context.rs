//! A context of a package  parsed from the disk.
use std::path::PathBuf;

use crate::{
    DuckContext, QuackResult,
    duck::util::duck_home::DuckHome,
    qp_bail,
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
    /// Create new [`PackageContext`].
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

    /// Create new [`PackageContext`] and ensure its name does not conflict with the global package name.
    pub fn new_not_global(project_root: PathBuf, ctx: &'duck DuckContext) -> QuackResult<Self> {
        let pcx = Self::new(project_root, ctx)?;
        if pcx.package.is_global() {
            qp_bail!(
                "The name {} is restricted to the global package",
                DuckHome::GLOBAL_PACKAGE_NAME
            );
        }
        Ok(pcx)
    }

    /// Get underlying [`Package`]
    pub fn package(&self) -> &Package {
        &self.package
    }

    /// Consume self, returning the underlying package.
    pub fn into_package(self) -> Package {
        self.package
    }

    /// Get [`VenvConfig`] of this [`PackageContext`]
    pub fn venv_config(&self) -> &VenvConfig {
        &self.venv_config
    }

    /// Get [`DuckContext`] used to create this [`PackageContext`]
    pub fn ctx(&self) -> &DuckContext {
        self.ctx
    }

    /// Is this the global package.
    pub fn is_global(&self) -> bool {
        self.package.is_global()
    }
}

impl From<PackageContext<'_>> for Package {
    fn from(value: PackageContext<'_>) -> Self {
        value.package
    }
}
