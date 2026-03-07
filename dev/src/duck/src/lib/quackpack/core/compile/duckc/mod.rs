mod compilation_type;
use std::process::Command;
use std::{ffi::OsStr, fmt};

pub use compilation_type::CompilationType;
use itertools::Itertools;

use super::compiler_package::CompilerPackage;
use crate::quackpack::core::Package;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::compiler_dag::CompilerDag;
use crate::quackpack::core::storage::freeze::FreezeDep;
use crate::util_common::path_ops_ext::{PathOpsExt, ShouldBlock};
use crate::{DuckCtx, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal};

#[non_exhaustive]
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub enum DuckcSubcommand {
    CompilePackage,
}

impl DuckcSubcommand {
    fn as_argument(&self) -> &'static str {
        match self {
            Self::CompilePackage => "compile_package",
        }
    }
}

#[derive(Debug)]
pub struct Duckc {
    program_name: StrId,
}

impl Duckc {
    pub fn new(ctx: &DuckCtx) -> Self {
        let _ = ctx;
        Self {
            program_name: "duckc".into(),
        }
    }

    pub fn compile(
        &self,
        graph: &CompilerDag,
        compilation_type: CompilationType,
        bcx: &BuildContext<'_>,
    ) -> QuackResult<()> {
        match compilation_type {
            CompilationType::OnlyRootPackage => self.compile_root_package_only(graph, bcx),
        }
    }

    fn compile_root_package_only(
        &self,
        graph: &CompilerDag,
        bcx: &BuildContext<'_>,
    ) -> QuackResult<()> {
        let this = graph.package(&graph.dag().root())?;
        let deps = graph.dag().dependencies_for_package(&graph.dag().root())?;
        bail_if_has_deps(deps.dependencies())?;
        bail_if_has_explicit_aliases(this)?;
        let this = this.package();
        let mut builder = DuckcProcessBuilder::new(self);
        builder
            .set_subcommand(DuckcSubcommand::CompilePackage)
            .set_package_name(this);

        let source_dir = this.source_directory();
        if !source_dir.is_dir() {
            qp_bail!(
                "package `{}` doesn't have a `src/` directory (expected `{}` to be a directory)",
                this.as_freeze_dep(),
                source_dir.display()
            )
        }
        builder.set_src_dir(this).set_package_artifacts_dir(this);

        let _lock = this.artifacts_directory().lock(ShouldBlock::Yes).with_context(|| format!("failed to acquire an exclusive lock for spawning a duckc in order to compile a package `{}`", this.as_freeze_dep()))?;
        bcx.duck_ctx
            .console()
            .info_verbose(format!("Running `{}`", builder));
        builder.execute(this.as_freeze_dep())?;
        Ok(())
    }
}

fn bail_if_has_deps(dependencies: &[FreezeDep]) -> QuackResult<()> {
    if !dependencies.is_empty() {
        qp_bail_internal!("external dependencies are not (yet) supported by duckc")
    }
    Ok(())
}

fn bail_if_has_explicit_aliases(package: &CompilerPackage) -> QuackResult<()> {
    let manifest = package.package().manifest();
    if manifest
        .dependencies()
        .all_dependencies()
        .values()
        .any(|dep| dep.is_aliased())
    {
        let desc = package.package().as_freeze_dep();
        qp_bail_internal!("package `{desc}` has aliased dependencies, which is not yet supported")
    }
    Ok(())
}

#[derive(Debug)]
pub struct DuckcProcessBuilder {
    inner: Command,
}

impl DuckcProcessBuilder {
    pub fn new(duckc: &Duckc) -> Self {
        Self {
            inner: Command::new(duckc.program_name),
        }
    }

    pub fn set_subcommand(&mut self, subcmd: DuckcSubcommand) -> &mut Self {
        self.inner.arg(subcmd.as_argument());
        self
    }

    pub fn set_package_name(&mut self, package: &Package) -> &mut Self {
        let name = package.manifest().root_description().name();
        self.inner.arg("-n").arg(name);
        self
    }

    pub fn set_src_dir(&mut self, package: &Package) -> &mut Self {
        self.inner.arg(package.source_directory());
        self
    }

    pub fn set_package_artifacts_dir(&mut self, package: &Package) -> &mut Self {
        let dir = package.artifacts_directory();
        self.inner.arg("-a").arg(dir);
        self
    }

    pub fn execute(&mut self, package_name: impl fmt::Display) -> QuackResult<()> {
        let code = self.inner.status().context("failed to spawn duckc")?;
        if !code.success() {
            qp_bail!("failed to compile package `{package_name}`")
        }
        Ok(())
    }
}

impl fmt::Display for DuckcProcessBuilder {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let command_name = self.inner.get_program().display();
        let args = self.inner.get_args().map(OsStr::display).join(" ");
        if !args.is_empty() {
            write!(f, "{command_name} {args}")
        } else {
            write!(f, "{command_name}")
        }
    }
}
