//! An implementation of [`UnitCompiler`], which creates a single task per package.

use tracing::instrument;

use super::{UnitCompiler, outputs};
use crate::QuackResult;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};

#[derive(Debug, Clone, Copy)]
pub struct DefaultUnitCompiler;

impl UnitCompiler for DefaultUnitCompiler {
    #[instrument(skip_all)]
    #[track_caller]
    fn pre_compilation(&self, graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) {
        let root = graph.root_unit();
        assert_eq!(
            root.artifacts_type(),
            ArtifactsType::Binary,
            "debug executor supports only compiling to the binary; got {:?}",
            root.artifacts_type(),
        );
    }

    #[instrument(skip_all)]
    fn create_tasks(
        &self,
        unit: &Unit,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        _bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>> {
        let task = create_task(unit, graph, layout)?;
        Ok(vec![task])
    }

    #[instrument(skip_all)]
    fn units_to_compile<'a>(
        &self,
        graph: &'a UnitGraph,
        _bcx: &BuildContext<'_, '_>,
    ) -> Vec<&'a Unit> {
        graph.units_sorted_by_id().iter().rev().collect()
    }
}

/// Create a task for a single [`Unit`].
#[instrument(skip_all)]
fn create_task(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<multipackage_schema::Task> {
    let strategy = match unit.artifacts_type() {
        ArtifactsType::Binary => {
            assert!(
                graph.is_root(unit),
                "only root should be compiled to binary"
            );
            multipackage_schema::PackageCompilationStrategy::Binary {
                output_file: outputs::unit_output(unit, graph, layout)?,
                linking_options: outputs::get_linker_options(unit, graph, layout)?,
            }
        }
        ArtifactsType::IsADependencyArtifact => {
            let layout = layout.for_dependency(unit, graph)?;
            multipackage_schema::PackageCompilationStrategy::Lib {
                output_file: layout.root_directory().join(unit.output_file_name()),
                archive_options: None,
            }
        }
        ArtifactsType::Dvm => unreachable!("DVM tasks should be handled by the `DvmExecutor`"),
        ArtifactsType::Library => unreachable!("library tasks are unsupported"),
    };
    Ok(multipackage_schema::Task {
        package_id: unit.unique_name().into(),
        strategy,
    })
}
