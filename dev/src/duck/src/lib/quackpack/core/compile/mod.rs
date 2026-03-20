//! Main entrypoint for compiling an entire project.
//!
//! Notable modules are:
//! - [`compiler_dag`][]: creating and modifying dependency graphs; notably, it checks for cycles,
//!   expands features, and removes disabled dependencies,
//! - [`duckc`][]: executing the compiler itself, it handles different compiler execution modes.
use tracing::debug;

use crate::{
    QuackResult, StrId,
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

#[derive(Debug)]
/// All informations required to compile a project.
pub struct BuildContext<'duck, 'ctx> {
    package: &'ctx PackageCtx<'duck>,
    freeze: VenvFreeze,
    storage: Storage,
    used_features: Vec<FeatureName>,
    profile: Profile,
}

impl<'duck, 'ctx> BuildContext<'duck, 'ctx> {
    pub fn new(
        package: &'ctx PackageCtx<'duck>,
        freeze: VenvFreeze,
        storage: Storage,
        used_features: Vec<FeatureName>,
        profile_name: StrId,
    ) -> QuackResult<Self> {
        Ok(Self {
            package,
            freeze,
            storage,
            used_features,
            profile: Profile::construct_profile(
                profile_name,
                package.package().manifest().profiles(),
            )?,
        })
    }
}

/// Compile project inside the [`BuildContext`].
pub fn compile(bcx: BuildContext<'_, '_>) -> QuackResult<()> {
    debug!("compiling `{bcx:?}`");
    let mut graph = CompilerDag::new_early(&bcx)?;
    graph.populate_features(&bcx.used_features)?;
    graph.remove_disabled_dependencies()?;
    let duckc = Duckc::new(bcx.package.ctx());
    duckc.compile(&graph, CompilationType::OnlyRootPackage, &bcx)?;
    Ok(())
}
