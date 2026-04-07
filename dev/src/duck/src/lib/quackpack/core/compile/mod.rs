//! Main entrypoint for compiling an entire project.
//!
//! Notable modules are:
//! - [`compiler_dag`][]: creating and modifying dependency graphs; notably, it checks for cycles,
//!   expands features, and removes disabled dependencies,
//! - [`duckc`][]: executing the compiler itself, it handles different compiler execution modes.
use tracing::debug;

use crate::{
    QuackResult,
    quackpack::core::{
        FeatureName, PackageCtx,
        compile::profiles::Profile,
        storage::{freeze::VenvFreeze, paths::Storage},
    },
};

pub mod compiler_dag;
pub mod compiler_package;
pub mod duckc;
pub mod profiles;
use compiler_dag::*;
use duckc::*;

const MISSING_DEPENDENCY_IN_DAG_MESSAGE: &str = "missing dependency in the map";
const MISSING_DEPENDENCY_IN_MANIFEST_MESSAGE: &str =
    "malformed manifest: missing dependency in the manifest";

#[derive(Debug)]
/// All informations required to compile a project.
pub struct BuildContext<'duck, 'ctx> {
    pub package: &'ctx PackageCtx<'duck>,
    pub freeze: VenvFreeze,
    pub storage: Storage,
    pub used_features: Vec<FeatureName>,
    pub profile: Profile,
}

/// Compile project inside the [`BuildContext`].
pub fn compile(bcx: BuildContext<'_, '_>) -> QuackResult<()> {
    debug!("compiling `{bcx:?}`");
    let mut graph = CompilerDag::new_early(&bcx)?;
    graph.populate_features(&bcx.used_features)?;
    graph.remove_disabled_dependencies();
    let duckc = Duckc::new(bcx.package.ctx());
    duckc.compile(&graph, CompilationType::OnlyRootPackage, &bcx)?;
    Ok(())
}
