//! An implementation of [`UnitTaskGenerator`] for DVM builds: every dependency is compiled into a
//! `dvm_lib`, and the root package into a `dvm_exe` that links them.

use tracing::instrument;

use super::UnitTaskGenerator;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::multipackage_schema;
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};
use crate::quackpack::core::compile::unit_runner::external_libs::{
    ExternalLibrariesFound, find_links_without_dvm_shared_libs,
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
            find_links_without_dvm_shared_libs(root, graph)
        {
            qp_bail!(
                "package {} links against `{links}`, which the DVM cannot load; list the shared \
                 objects to load instead in `metadata.dvm-shared-libs`",
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

    fn should_run(&self, _unit: &Unit, _graph: &UnitGraph, _bcx: &BuildContext<'_, '_>) -> bool {
        true
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
        ArtifactsType::Dvm => multipackage_schema::PackageCompilationStrategy::DvmExe {
            output_file: outputs::unit_output(unit, graph, layout)?,
            dvm_linking_options: outputs::get_dvm_linking_options(unit, graph, layout)?,
        },
        ArtifactsType::DvmDependency => multipackage_schema::PackageCompilationStrategy::DvmLib {
            output_file: outputs::unit_output(unit, graph, layout)?,
        },
        task => unreachable!("should create only DVM tasks (attempted to create for `{task:?}`)"),
    };
    Ok(multipackage_schema::Task {
        package_id: unit.unique_name().into(),
        strategy,
    })
}
