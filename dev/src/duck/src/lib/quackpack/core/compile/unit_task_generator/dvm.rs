//! An implementation of [`UnitTaskGenerator`], which invokes duckc only once, for a root package with
//! `dvm` task.

use tracing::instrument;

use super::UnitTaskGenerator;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{Unit, UnitType};
use crate::quackpack::core::compile::unit_runner::external_libs::{
    ExternalLibrariesFound, has_external_libraries,
};
use crate::quackpack::core::compile::unit_runner::outputs;
use crate::{QuackResult, qp_bail};

#[derive(Debug, Clone, Copy)]
pub struct DvmTaskGenerator;

impl UnitTaskGenerator for DvmTaskGenerator {
    #[instrument(skip_all)]
    #[track_caller]
    fn pre_compilation(&self, graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> QuackResult<()> {
        let root = graph.root_unit();
        assert_eq!(
            root.unit_type(),
            UnitType::Binary,
            "dvm executor should only compile binary packages; got {:?}",
            root.unit_type()
        );
        if let Some(ExternalLibrariesFound { unit, links }) = has_external_libraries(root, graph) {
            qp_bail!(
                "package {} links against `{links}`, which is not supported on the DVM",
                unit.descriptive_name()
            )
        }
        Ok(())
    }

    #[instrument(skip_all)]
    fn create_tasks(
        &self,
        unit: &Unit,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>> {
        let task = create_task(unit, graph, layout, bcx)?;
        Ok(vec![task])
    }

    fn should_run(&self, unit: &Unit, graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> bool {
        graph.is_root(unit)
    }
}

/// Create a task for a single [`Unit`].
#[instrument(skip_all)]
fn create_task(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<multipackage_schema::Task> {
    assert!(
        graph.is_root(unit),
        "`DvmExecutor` should create task only for the root `Unit`"
    );
    let strategy = match unit.unit_type() {
        // QuackPack only emits DVM executables; `DvmLib` is produced solely by the
        // C++ std library path, so it is intentionally unreachable here.
        UnitType::Binary => multipackage_schema::PackageCompilationStrategy::DvmExe {
            output_file: outputs::unit_output(unit, graph, layout, bcx)?,
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
