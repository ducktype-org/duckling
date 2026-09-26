//! An implementation of [`UnitTaskGenerator`], which invokes duckc only once, for a root package with
//! `dvm` task.

use tracing::instrument;

use super::UnitTaskGenerator;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};
use crate::quackpack::core::compile::unit_runner::external_libs::{
    ExternalLibrariesFound, gather_dvm_shared_libs, has_dvm_incompatible_libraries,
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
            root.artifacts_type(),
            ArtifactsType::Dvm,
            "dvm executor should only compile DVM packages; got {:?}",
            root.artifacts_type()
        );
        if let Some(ExternalLibrariesFound { unit, links }) =
            has_dvm_incompatible_libraries(root, graph)
        {
            qp_bail!(
                "package {} links against `{links}`, which is not supported on the DVM; \
                 declare `metadata.dvm-shared-libs` to provide a shared object instead",
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
) -> QuackResult<multipackage_schema::Task> {
    assert!(
        graph.is_root(unit),
        "`DvmExecutor` should create task only for the root `Unit`"
    );
    let strategy = match unit.artifacts_type() {
        // QuackPack only emits DVM executables; `DvmLib` is produced solely by the
        // C++ std library path, so it is intentionally unreachable here.
        ArtifactsType::Dvm => {
            let shared_libraries: Vec<String> = gather_dvm_shared_libs(unit, graph)
                .into_iter()
                .map(|library| library.to_string())
                .collect();
            multipackage_schema::PackageCompilationStrategy::DvmExe {
                output_file: outputs::unit_output(unit, graph, layout)?,
                dvm_linking_options: (!shared_libraries.is_empty())
                    .then_some(multipackage_schema::DvmLinkingOptions { shared_libraries }),
            }
        }
        task => unreachable!(
            "should create only DVM task for the root (attempted to create for `{task:?}`)"
        ),
    };
    Ok(multipackage_schema::Task {
        package_id: unit.unique_name().into(),
        strategy,
    })
}
