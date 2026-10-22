// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! An implementation of [`UnitTaskGenerator`], which creates a single task per package.

use tracing::instrument;

use crate::QuackResult;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph};
use crate::quackpack::core::compile::unit::{Unit, UnitType};
use crate::quackpack::core::compile::unit_runner::external_libs::validate_external_libraries;
use crate::quackpack::core::compile::unit_runner::outputs;
use crate::quackpack::core::compile::unit_task_generator::UnitTaskGenerator;

/// Default task generator for LLVM.
/// Creates a single task for each [`Unit`].
#[derive(Debug, Clone, Copy)]
pub struct DefaultTaskGenerator;

impl UnitTaskGenerator for DefaultTaskGenerator {
    #[instrument(skip_all)]
    #[track_caller]
    fn pre_compilation(&self, graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> QuackResult<()> {
        let root = graph.root_unit();
        assert_eq!(
            root.unit_type(),
            UnitType::Binary,
            "debug executor supports only compiling to the binary; got {:?}",
            root.unit_type(),
        );
        validate_external_libraries(graph.root_id(), graph)?;
        Ok(())
    }

    #[instrument(skip_all)]
    fn create_tasks(
        &self,
        unit: &Unit,
        unit_id_in_graph: GraphNodeId,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>> {
        let task = create_task(unit, unit_id_in_graph, graph, layout, bcx)?;
        Ok(vec![task])
    }

    fn should_run(&self, _unit: &Unit, _graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> bool {
        true
    }
}

/// Create a task for a single [`Unit`].
#[instrument(skip_all)]
fn create_task(
    unit: &Unit,
    unit_id_in_graph: GraphNodeId,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<multipackage_schema::Task> {
    let strategy = match unit.unit_type() {
        UnitType::Binary => {
            assert!(
                graph.is_root(unit),
                "only root should be compiled to binary"
            );
            multipackage_schema::PackageCompilationStrategy::Binary {
                output_file: outputs::unit_output(unit, unit_id_in_graph, graph, layout, bcx)?,
                linking_options: outputs::get_linker_options(unit_id_in_graph, graph, layout, bcx)?,
            }
        }
        UnitType::Dependency => {
            let layout = layout.for_dependency(unit_id_in_graph, graph)?;
            multipackage_schema::PackageCompilationStrategy::Lib {
                output_file: layout.root_directory().join(unit.output_file_name(bcx)),
                archive_options: None,
            }
        }
        UnitType::Library => unreachable!("library tasks are unsupported"),
    };
    Ok(multipackage_schema::Task {
        package_id: unit.pkg_unique_name().into(),
        strategy,
    })
}
