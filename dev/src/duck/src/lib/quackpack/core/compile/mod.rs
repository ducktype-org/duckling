use tracing::debug;

use crate::{
    DuckCtx, QuackResult, StrId,
    quackpack::core::{
        FeatureName, PackageCtx,
        storage::{freeze::VenvFreeze, paths::Storage},
    },
};

pub mod compiler_graph;
pub mod compiler_package;
pub mod duckc;
use compiler_graph::*;
use duckc::*;

#[derive(Debug)]
pub struct BuildContext<'duck> {
    pub duck_ctx: &'duck DuckCtx,
    pub package: &'duck PackageCtx<'duck>,
    pub freeze: VenvFreeze,
    pub storage: Storage,
    pub used_features: Vec<FeatureName>,
    pub profile: StrId,
}

pub fn compile<'duck>(bcx: BuildContext<'duck>) -> QuackResult<()> {
    debug!("compiling `{bcx:?}`");
    let mut graph = CompilerGraph::new_early(&bcx)?;
    graph.populate_features(&bcx.used_features)?;
    graph.remove_disabled_dependencies()?;
    let duckc = Duckc::new(bcx.duck_ctx);
    duckc.compile(&graph, CompilationType::OnlyRootPackage, &bcx)?;
    Ok(())
}
