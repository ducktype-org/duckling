//! A context of a package  parsed from the disk.
use std::path::{Path, PathBuf};

use super::lints::warnings::Warnings;
use super::script::Script;
use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::package_loader::PackageLoader;
use crate::quackpack::core::script::StandaloneScript;
use crate::quackpack::core::{self, AnyPackage};
use crate::{DuckContext, QuackResult, qp_bail};

#[derive(Debug)]
/// A context of a package  parsed from the disk.
pub struct PackageContext<'duck> {
    package: AnyPackage,
    ctx: &'duck DuckContext,

    /// Warnings created while this package had been parsed.
    warnings: Warnings,
}

impl<'duck> PackageContext<'duck> {
    /// Create new [`PackageContext`].
    #[tracing::instrument(skip_all)]
    pub fn new(project_root: PathBuf, ctx: &'duck DuckContext) -> QuackResult<Self> {
        let (package, warnings) =
            core::parse_manifest(&project_root.join(PackageLoader::MANIFEST_NAME), ctx)?;
        Ok(Self {
            package: AnyPackage::Package(package),
            ctx,
            warnings,
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

    /// Create a new [`PackageContext`] for a standalone script.
    #[tracing::instrument(skip_all)]
    pub fn new_standalone_script(path: &Path, ctx: &'duck DuckContext) -> QuackResult<Self> {
        let (frontmatter, warnings) = core::parse_frontmatter(path, ctx)?;
        let script = StandaloneScript::new(frontmatter);
        Ok(Self::new_script(script.into(), ctx, warnings))
    }

    /// Create a new [`PackageContext`] for a script.
    pub fn new_script(script: Script, ctx: &'duck DuckContext, warnings: Warnings) -> Self {
        Self {
            package: AnyPackage::Script(script),
            ctx,
            warnings,
        }
    }

    /// Get underlying [`AnyPackage`] as a reference.
    pub fn package(&self) -> &AnyPackage {
        &self.package
    }

    /// Transform into the underlying [`AnyPackage`], discarding any warnings.
    pub fn into_package(self) -> AnyPackage {
        self.package
    }

    /// Transform into the underlying [`AnyPackage`] and keep the warnings.
    pub fn into_package_and_warnings(self) -> (AnyPackage, Warnings) {
        (self.package, self.warnings)
    }

    /// Get [`DuckContext`] used to create this [`PackageContext`]
    pub fn ctx(&self) -> &DuckContext {
        self.ctx
    }

    /// Is this the global package.
    pub fn is_global(&self) -> bool {
        self.package.is_global()
    }

    /// Get the path to the storage.
    pub fn storage_path(&self) -> &Path {
        self.package().venv().storage_path()
    }

    /// Emit collected warnings.
    pub fn emit_warnings(&self) -> QuackResult<()> {
        self.warnings.emit_warnings(self.ctx())
    }
}
