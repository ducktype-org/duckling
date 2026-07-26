//! A context of a package  parsed from the disk.
use std::path::{Path, PathBuf};

use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::package_loader::PackageLoader;
use crate::quackpack::core::{self, AnyPackage};
use crate::{DuckContext, QuackResult, qp_bail, qp_bail_internal};

#[derive(Debug)]
/// A context of a package  parsed from the disk.
pub struct PackageContext<'duck> {
    package: AnyPackage,
    ctx: &'duck DuckContext,
}

impl<'duck> PackageContext<'duck> {
    /// Create new [`PackageContext`].
    #[tracing::instrument(skip_all)]
    pub fn new(project_root: PathBuf, ctx: &'duck DuckContext) -> QuackResult<Self> {
        let package = core::parse_manifest(&project_root.join(PackageLoader::MANIFEST_NAME), ctx)?;
        Ok(Self {
            package: AnyPackage::Package(package),
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

    /// Try create new [`PackageContext`] from a script with a frontmatter at `path`.
    /// If the script does not contain a frontmatter, returns `Ok(None)`.
    #[tracing::instrument(skip_all)]
    pub fn try_new_from_frontmatter(
        path: PathBuf,
        ctx: &'duck DuckContext,
    ) -> QuackResult<Option<Self>> {
        let Some(frontmatter) = core::try_parse_frontmatter(path.clone(), ctx)? else {
            return Ok(None);
        };
        Ok(Some(Self {
            package: AnyPackage::Script(frontmatter),
            ctx,
        }))
    }

    /// As [`Self::try_new_from_frontmatter`], but bails internally when there is no frontmatter at `path`.
    #[tracing::instrument(skip_all)]
    pub fn new_from_frontmatter(
        path: PathBuf,
        ctx: &'duck DuckContext,
    ) -> QuackResult<Option<Self>> {
        let Some(frontmatter) = core::try_parse_frontmatter(path.clone(), ctx)? else {
            qp_bail_internal!(
                "tried to construct a frontmatter package context for something that is not a frontmatter"
            )
        };
        Ok(Some(Self {
            package: AnyPackage::Script(frontmatter),
            ctx,
        }))
    }

    /// Get underlying [`AnyPackage`] as a reference.
    pub fn package(&self) -> &AnyPackage {
        &self.package
    }

    /// Transform into the underlying [`AnyPackage`].
    pub fn into_package(self) -> AnyPackage {
        self.package
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
}
