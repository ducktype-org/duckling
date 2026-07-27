//! An implementation of [`Executor`], which creates a single task per package.

use tracing::instrument;

use super::{Executor, ExecutorOutput, compile_single_unit_with_tasks, unit_output};
use crate::QuackResult;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::{ArtifactsLayout, ProfileLayout};
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};

#[derive(Debug, Clone, Copy)]
pub struct DvmExecutor;

impl Executor for DvmExecutor {
    fn compile(&self, graph: UnitGraph, bcx: &BuildContext<'_, '_>) -> QuackResult<ExecutorOutput> {
        compile(graph, bcx)
    }
}

/// Compile the `graph` in debug-mode.
///
/// This means:
/// 1. each `deps.json` has only one task,
/// 2. we compile each [`Unit`] independently, and each gets different artifacts,
/// 3. root is compiled last.
#[instrument(skip_all)]
fn compile(graph: UnitGraph, bcx: &BuildContext<'_, '_>) -> QuackResult<ExecutorOutput> {
    let root = graph.root_unit();
    assert_eq!(
        root.artifacts_type(),
        ArtifactsType::Dvm,
        "dvm executor should only compile DVM packages"
    );
    let artifacts_layout = root
        .root_package()
        .package()
        .get_package()
        .artifacts_layout();
    let profile_layout = artifacts_layout.for_profile(bcx.profile);
    compile_unit(root, &graph, &profile_layout, bcx)?;
    let output = unit_output(root, &graph, &profile_layout)?;
    Ok(ExecutorOutput {
        root: (root.clone(), output),
    })
}

#[instrument(skip_all)]
/// Compile only a single [`Unit`], in a [`compile`] favour.
fn compile_unit(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &impl ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<()> {
    let task = create_task(unit, graph, layout)?;
    compile_single_unit_with_tasks(unit, graph, layout, bcx, vec![task])
}

/// Create a task for a single [`Unit`].
#[instrument(skip_all)]
fn create_task(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &impl ProfileLayout,
) -> QuackResult<multipackage_schema::Task> {
    assert!(
        graph.is_root(unit),
        "`DvmExecutor` should create task only for the root `Unit`"
    );
    let strategy = match unit.artifacts_type() {
        // QuackPack only emits DVM executables; `DvmLib` is produced solely by the
        // C++ std library path, so it is intentionally unreachable here.
        ArtifactsType::Dvm => multipackage_schema::PackageCompilationStrategy::DvmExe {
            output_file: unit_output(unit, graph, layout)?,
        },
        task => unreachable!(
            "should create only DVM task for the root (attempted to create for `{task:?}`)"
        ),
    };
    Ok(multipackage_schema::Task {
        package_id: unit.unique_name().into(),
        strategy,
    })
}
