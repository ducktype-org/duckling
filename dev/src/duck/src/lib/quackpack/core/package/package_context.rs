// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! A context of a package  parsed from the disk.
use std::path::{Path, PathBuf};

use super::script::Script;
use crate::duck::util::duck_home::DuckHome;
use crate::quackpack::core::full_identity::{FullIdentity, FullOrigin};
use crate::quackpack::core::lints::errors::Errors;
use crate::quackpack::core::lints::warnings::Warnings;
use crate::quackpack::core::package_loader::PackageLoader;
use crate::quackpack::core::script::StandaloneScript;
use crate::quackpack::core::{self, AnyPackage, PackageId};
use crate::{DuckContext, QuackResult, qp_bail};

#[derive(Debug)]
/// A context of a package  parsed from the disk.
pub struct PackageContext<'duck> {
    package: AnyPackage,
    ctx: &'duck DuckContext,

    /// Warnings created while this package had been parsed.
    warnings: Warnings,

    /// Errors created while this package had been parsed.
    errors: Errors,
}

impl<'duck> PackageContext<'duck> {
    /// Create new [`PackageContext`].
    #[tracing::instrument(skip_all)]
    pub fn new(project_root: PathBuf, ctx: &'duck DuckContext) -> QuackResult<Self> {
        let (package, warnings, errors) =
            core::parse_manifest(&project_root.join(PackageLoader::MANIFEST_NAME), ctx)?;
        Ok(Self {
            package: AnyPackage::Package(package),
            ctx,
            warnings,
            errors,
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
        let (frontmatter, warnings, errors) = core::parse_frontmatter(path, ctx)?;
        let script = StandaloneScript::new(frontmatter);
        Ok(Self::new_script(script.into(), ctx, warnings, errors))
    }

    /// Create a new [`PackageContext`] for a script.
    pub fn new_script(
        script: Script,
        ctx: &'duck DuckContext,
        warnings: Warnings,
        errors: Errors,
    ) -> Self {
        Self {
            package: AnyPackage::Script(script),
            ctx,
            warnings,
            errors,
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
    pub fn into_inner(self) -> (AnyPackage, Warnings, Errors) {
        (self.package, self.warnings, self.errors)
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

    /// Emit collected diagnostics.
    pub fn emit_diagnostics(&self) -> QuackResult<()> {
        self.warnings.emit_warnings(self.ctx())?;
        self.errors.emit_errors(self.ctx())
    }

    /// Returns the [`PackageId`] associated with this package treated as a local one.
    pub fn local_package_id(&self) -> QuackResult<PackageId> {
        let origin = FullOrigin::for_local(self.package().root())?;
        let identity = FullIdentity::new(self.package().name(), origin);
        Ok(PackageId::new(identity, self.package().version()))
    }
}
