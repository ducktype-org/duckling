//! An implementation of [`UnitCompiler`], which invokes duckc only once, for a root package with
//! `dvm` task.

use tracing::instrument;

use super::external_libs::{ExternalLibrariesFound, has_external_libraries};
use super::{UnitCompiler, outputs};
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};
use crate::{QuackResult, qp_bail};

#[derive(Debug, Clone, Copy)]
pub struct DvmUnitCompiler;

impl UnitCompiler for DvmUnitCompiler {
    #[instrument(skip_all)]
    #[track_caller]
    fn pre_compilation(&self, graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> QuackResult<()> {
        let root = graph.root_unit();
        assert_eq!(
            root.artifacts_type(),
            ArtifactsType::Dvm,
            "dvm executor should only compile DVM packages; got {:?}",
            root.artifacts_type()
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
        _bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>> {
        let task = create_task(unit, graph, layout)?;
        Ok(vec![task])
    }

    fn should_compile(&self, unit: &Unit, graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> bool {
        graph.is_root(unit)
    }
}

/// Create a task for a single [`Unit`].
#[instrument(skip_all)]
fn create_task(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<multipackage_schema::Task> {
    assert!(
        graph.is_root(unit),
        "`DvmExecutor` should create task only for the root `Unit`"
    );
    let strategy = match unit.artifacts_type() {
        // QuackPack only emits DVM executables; `DvmLib` is produced solely by the
        // C++ std library path, so it is intentionally unreachable here.
        ArtifactsType::Dvm => multipackage_schema::PackageCompilationStrategy::DvmExe {
            output_file: outputs::unit_output(unit, graph, layout)?,
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
