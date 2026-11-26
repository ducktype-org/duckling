// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! An implementation of [`UnitTaskGenerator`] for DVM builds: every dependency is compiled into a
//! `dvm_lib`, and the root package into a `dvm_exe` that links them.

use crate::core::compile::BuildContext;
use crate::core::compile::artifacts_layout::ProfileLayout;
use crate::core::compile::duckc::multipackage_schema;
use crate::core::compile::unit::graph::{GraphNodeId, UnitGraph};
use crate::core::compile::unit::{Unit, UnitType};
use crate::core::compile::unit_runner::external_libs::{
    ExternalLibrariesFound, has_external_libraries,
};
use crate::core::compile::unit_runner::outputs;
use crate::core::compile::unit_task_generator::UnitTaskGenerator;
use crate::{QuackResult, qp_bail};

/// Task generator for DVM.
#[derive(Debug, Clone, Copy)]
pub struct DvmTaskGenerator;

impl UnitTaskGenerator for DvmTaskGenerator {
    #[track_caller]
    fn pre_compilation(&self, graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> QuackResult<()> {
        let root = graph.root_unit();
        assert_eq!(
            root.unit_type(),
            UnitType::Binary,
            "dvm executor should only compile binary packages; got {:?}",
            root.unit_type()
        );
        if let Some(ExternalLibrariesFound { unit, lib }) =
            has_external_libraries(graph.root_id(), graph)
        {
            qp_bail!(
                "package {} links against `{lib}`, which is not supported on the DVM",
                unit.pkg_descriptive_name()
            )
        }
        Ok(())
    }

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
}

/// Create a task for a single [`Unit`].
fn create_task(
    unit: &Unit,
    unit_id_in_graph: GraphNodeId,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<multipackage_schema::Task> {
    let strategy = match unit.unit_type() {
        UnitType::Binary => multipackage_schema::PackageCompilationStrategy::DvmExe {
            output_file: outputs::unit_output(unit, unit_id_in_graph, graph, layout, bcx)?,
            dvm_linking_options: outputs::get_dvm_linking_options(
                unit_id_in_graph,
                graph,
                layout,
                bcx,
            )?,
        },
        UnitType::Dependency => multipackage_schema::PackageCompilationStrategy::DvmLib {
            output_file: outputs::unit_output(unit, unit_id_in_graph, graph, layout, bcx)?,
            // The executable links every dependency.
            dvm_linking_options: None,
        },
        UnitType::Library => unreachable!("library tasks are unsupported"),
    };
    Ok(multipackage_schema::Task {
        package_id: unit.pkg_unique_name().into(),
        strategy,
    })
}
