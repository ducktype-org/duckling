use std::path::PathBuf;

use crate::{
    QpCtx, QuackResult,
    quackpack::core::{self, Package, package_loader::PackageLoader, venv_config::VenvConfig},
};

#[derive(Debug)]
/// Context of a parsed package on a disk.
pub struct PackageCtx<'duck> {
    package: Package,
    venv_config: VenvConfig,
    ctx: &'duck QpCtx<'duck>,
}

impl<'duck> PackageCtx<'duck> {
    /// Create new [`PackageCtx`]
    pub fn new(project_root: PathBuf, ctx: &'duck QpCtx<'duck>) -> QuackResult<Self> {
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

    /// Get [`QpCtx`] used to create this [`PackageCtx`]
    pub fn ctx(&self) -> &QpCtx<'duck> {
        self.ctx
    }
}
