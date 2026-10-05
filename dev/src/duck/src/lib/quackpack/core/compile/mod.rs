//! Main entrypoint for compiling an entire project.
//!
//! Notable modules are:
//! - [`early_graph`][]: creating and modifying dependency graphs; notably, it checks for cycles,
//!   expands features, and removes disabled dependencies,
//! - [`duckc`][]: executing the compiler itself, it handles different compiler execution modes.
use std::fmt;

use tracing::info;

use self::early_graph::creating_graph::create_early_graph_from_bcx;
use self::profiles::Profile;
use self::unit::graph::lower_early_graph;
use self::unit_runner::{CompilationOutput, UnitRunner};
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::{AnyPackage, FeatureName, PackageContext, PackageId};
use crate::{QuackResult, qp_bail_internal};

pub mod artifacts_layout;
pub mod compiler_package;
pub mod duckc;
pub mod early_graph;
pub mod profiles;
pub mod unit;
pub mod unit_new;
pub mod unit_runner;
pub mod unit_runner_new;
pub mod unit_task_generator;
pub mod unit_task_generator_new;

/// A common message for panicking when a manifest is missing a dependency.
pub fn missing_depenendcy_in_manifest(root_name: &str, dep: &str, context: &dyn fmt::Debug) -> ! {
    panic!(
        "malformed manifest of `{root_name}`: missing dependency `{dep}` in the manifest {context:#?}"
    )
}

/// A common message for panicking when any graph is missing a key.
pub fn missing_depenendcy_in_graph(id: Identity, context: &dyn fmt::Debug) -> ! {
    panic!("missing dependency `{id}` in the graph {context:#?}")
}

#[derive(Debug)]
/// All informations required to compile a project.
pub struct BuildContext<'duck, 'ctx> {
    /// Package to build or venv of the script.
    pub pcx: &'ctx PackageContext<'duck>,
    pub root_identity: Identity,
    pub freeze: SolverFreeze,
    pub storage: Storage,
    pub used_features: Vec<FeatureName>,
    pub profile: Profile,
    pub shared: bool,
    pub jobs: usize,
}

/// Compile project inside the [`BuildContext`].
/// Note:
/// -----
/// `pkgs` should be all packages present in the freeze, including the root package.
#[tracing::instrument(skip_all)]
pub fn compile(
    bcx: BuildContext<'_, '_>,
    pkgs: Vec<(PackageId, AnyPackage)>,
) -> QuackResult<CompilationOutput> {
    info!(?bcx, "compiling");
    // @TODO: #2900 Unmock this.
    if bcx.pcx.package().is_script() {
        qp_bail_internal!("compiling scripts via Unit and manifest.json is not (yet) supported")
    }
    let graph = create_early_graph_from_bcx(&bcx, pkgs)?;
    let unit_graph = lower_early_graph(graph, &bcx);
    let runner = UnitRunner::new(unit_graph, &bcx);
    runner.run()
}
