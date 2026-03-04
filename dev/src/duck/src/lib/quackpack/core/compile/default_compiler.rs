use std::{ffi::OsStr, process::Command};

use itertools::Itertools;

use super::Compiler;
use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal,
    quackpack::core::compile::compiler_package::CompilerPackage,
    util_common::path_ops_ext::{PathOpsExt, ShouldBlock},
};

#[non_exhaustive]
#[derive(Debug, Clone, Copy, Eq, PartialEq)]
enum CompilationKind {
    CompilePackage,
}

impl CompilationKind {
    fn as_duckc_subcommand(&self) -> &'static str {
        match self {
            Self::CompilePackage => "compile_package",
        }
    }
}

pub struct DefaultCompiler<'duck> {
    pub duck_ctx: &'duck DuckCtx,
}

impl DefaultCompiler<'_> {
    fn new_default_bulder() -> Command {
        Command::new("duckc")
    }

    fn set_compilation_kind(builder: &mut Command, kind: CompilationKind) {
        builder.arg(kind.as_duckc_subcommand());
    }

    fn set_package_name(builder: &mut Command, package: &CompilerPackage) {
        let name = package.package().manifest().root_description().name();
        builder.arg("-n").arg(name);
    }

    fn set_src_dir(builder: &mut Command, package: &CompilerPackage) {
        builder.arg(package.package().source_directory());
    }

    fn set_package_artifacts_dir(builder: &mut Command, package: &CompilerPackage) {
        let dir = package.package().artifacts_dir();
        builder.arg("-a").arg(dir);
    }

    fn set_profile_arguments(builder: &mut Command, package: &CompilerPackage, profile: StrId) {
        if let Some(profile) = package.package().manifest().profiles().options_for(profile) {
            builder.args(profile);
        }
    }
}

impl<'duck> Compiler for DefaultCompiler<'duck> {
    fn compile_package(
        &self,
        package: &CompilerPackage,
        dependencies: &[&CompilerPackage],
        profile: StrId,
    ) -> QuackResult<()> {
        bail_if_has_unsupported_features_by_duckc(package, dependencies)?;
        let mut builder = DefaultCompiler::new_default_bulder();
        DefaultCompiler::set_compilation_kind(&mut builder, CompilationKind::CompilePackage);
        DefaultCompiler::set_package_name(&mut builder, package);
        let source_dir = package.package().source_directory();
        if !source_dir.is_dir() {
            qp_bail!(
                "package `{}` doesn't have a `src/` directory (expected `{}` to be a directory)",
                package.package().as_freeze_dep(),
                source_dir.display()
            )
        }
        DefaultCompiler::set_src_dir(&mut builder, package);
        DefaultCompiler::set_package_artifacts_dir(&mut builder, package);
        DefaultCompiler::set_profile_arguments(&mut builder, package, profile);
        // Duckc doesn't support parallel compilations on the same artifacts directory.
        let _lock = package.package().artifacts_dir().lock(ShouldBlock::Yes).with_context(|| format!("failed to acquire an exclusive lock for spawning a duckc in order to compile a package `{}`", package.package().as_freeze_dep()))?;
        self.duck_ctx
            .console()
            .info_verbose(format!("Running `{}`", get_command_as_string(&builder)));
        let code = builder.status().context("failed to spawn duckc")?;
        if !code.success() {
            qp_bail!(
                "failed to compile package `{}`",
                package.package().as_freeze_dep()
            )
        }
        Ok(())
    }
}

fn bail_if_has_unsupported_features_by_duckc(
    package: &CompilerPackage,
    dependencies: &[&CompilerPackage],
) -> QuackResult<()> {
    bail_if_has_deps(dependencies)?;
    bail_if_has_explicit_aliases(package)?;
    Ok(())
}

fn bail_if_has_deps(dependencies: &[&CompilerPackage]) -> QuackResult<()> {
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

fn get_command_as_string(command: &Command) -> String {
    let base = command.get_program().display();
    let args = command.get_args().map(OsStr::display).join(" ");
    if !args.is_empty() {
        format!("{base} {args}")
    } else {
        base.to_string()
    }
}
