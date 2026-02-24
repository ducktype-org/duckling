use tracing::debug;

use crate::{
    DuckCtx, QuackResult, StrId,
    quackpack::core::{
        FeatureName, PackageCtx,
        compile::default_compiler::DefaultCompiler,
        storage::{freeze::VenvFreeze, paths::Storage},
    },
};

pub mod compiler_graph;
pub mod compiler_package;
pub mod default_compiler;
use compiler_graph::*;
use compiler_package::*;

#[derive(Debug)]
pub struct BuildContext<'duck> {
    pub duck_ctx: &'duck DuckCtx,
    pub package: &'duck PackageCtx<'duck>,
    pub freeze: VenvFreeze,
    pub storage: Storage,
    pub used_features: Vec<FeatureName>,
    pub profile: StrId,
}

pub fn compile<'duck>(bctx: BuildContext<'duck>) -> QuackResult<()> {
    debug!("compiling `{bctx:?}`");
    let mut graph = CompilerGraph::new_early(&bctx)?;
    graph.populate_features(&bctx.used_features)?;
    graph.remove_disabled_dependencies()?;
    let compiler = DefaultCompiler {
        duck_ctx: bctx.duck_ctx,
    };
    graph.compile(&compiler, &bctx)?;
    Ok(())
}

pub trait Compiler: Send + Sync {
    fn compile_package(
        &self,
        package: &CompilerPackage,
        dependencies: &[&CompilerPackage],
        profile: StrId,
    ) -> QuackResult<()>;
}
