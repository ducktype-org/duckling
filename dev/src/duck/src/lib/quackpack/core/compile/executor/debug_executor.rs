//! An implementation of [`Executor`], which creates a single task per package.

use tracing::instrument;

use super::{
    Executor, ExecutorOutput, compile_single_unit_with_tasks, get_linker_options, unit_output,
};
use crate::QuackResult;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};

#[derive(Debug, Clone, Copy)]
pub struct DebugExecutor;

impl Executor for DebugExecutor {
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
        ArtifactsType::Binary,
        "debug executor supports only compiling to the binary"
    );
    let artifacts_layout = graph
        .root_unit()
        .root_package()
        .package()
        .get_package()
        .artifacts_directory();
    let profile_layout = artifacts_layout.for_profile(&bcx.profile.name);
    // [OLD COMMENT] We explicitly compile `root` at the end.
    // [UPDATED/VALID COMMENT] Units are sorted by ID, and root has ID 0, so in reverse we'll
    // compile root last
    for unit in graph.units_sorted_by_id().iter().rev() {
        compile_unit(unit, &graph, &profile_layout, bcx)?;
    }
    let output = unit_output(root, &graph, &profile_layout);
    Ok(ExecutorOutput {
        root: (root.clone(), output),
    })
}

#[instrument(skip_all)]
/// Compile only a single [`Unit`], in a [`compile`] favour.
fn compile_unit(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<()> {
    let task = create_task(unit, graph, layout);
    compile_single_unit_with_tasks(unit, graph, layout, bcx, vec![task])
}

/// Create a task for a single [`Unit`].
#[instrument(skip_all)]
fn create_task(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
) -> multipackage_schema::Task {
    let strategy = match unit.artifacts_type() {
        ArtifactsType::Binary => {
            assert!(
                graph.is_root(unit),
                "only root should be compiled to binary"
            );
            multipackage_schema::PackageCompilationStrategy::Binary {
                output_file: unit_output(unit, graph, layout),
                linking_options: get_linker_options(unit, graph, layout),
            }
        }
        ArtifactsType::IsADependencyArtifact => {
            let layout = layout.for_dependency(&unit.unique_name());
            multipackage_schema::PackageCompilationStrategy::Lib {
                output_file: layout.root_directory().join(unit.output_file_name()),
                archive_options: None,
            }
        }
        ArtifactsType::Dvm => unreachable!("DVM tasks should be handled by the `DvmExecutor`"),
        ArtifactsType::Library => unreachable!("library tasks are unsupported"),
    };
    multipackage_schema::Task {
        package_id: unit.unique_name().into(),
        strategy,
    }
}
