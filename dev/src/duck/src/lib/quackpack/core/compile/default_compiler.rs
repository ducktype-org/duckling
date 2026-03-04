use std::process::Command;

use super::Compiler;
use crate::{
    DuckCtx, QuackResult, QuackResultContext, StrId, qp_bail, qp_bail_internal,
    quackpack::core::compile::compiler_package::CompilerPackage,
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
        DefaultCompiler::set_package_artifacts_dir(&mut builder, package);
        DefaultCompiler::set_profile_arguments(&mut builder, package, profile);
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
