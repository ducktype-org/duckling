//! An implementation of [`Executor`], which creates a single task per package.

use std::process::ExitStatus;

use tracing::instrument;

use super::{Executor, ExecutorOutput, unit_output};
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::artifacts_layout::ProfileLayout;
use crate::quackpack::core::compile::duckc::process_builder::DuckcSubcommand;
use crate::quackpack::core::compile::duckc::{Duckc, multipackage_schema};
use crate::quackpack::core::compile::executor::{
    collect_packages, get_linker_options, write_manifest,
};
use crate::quackpack::core::compile::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::{ArtifactsType, Unit};
use crate::{QuackResult, QuackResultContext, qp_bail};

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
    // We explicitly compile `root` at the end.
    for unit in graph.any_units_order().filter(|dep| !graph.is_root(dep)) {
        compile_unit(unit, &graph, &profile_layout, bcx)?;
    }
    compile_unit(root, &graph, &profile_layout, bcx)?;
    let output = unit_output(root, &graph, &profile_layout);
    Ok(ExecutorOutput {
        root: (root.clone(), output),
    })
}

/// Compile only a single [`Unit`], in a [`compile`] favour.
#[instrument(skip_all)]
fn compile_unit(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<()> {
    let status = compile_unit_impl(unit, graph, layout, bcx).with_context(|| {
        format!(
            "failed to compile `{}`",
            unit.root_package().package().manifest().name()
        )
    })?;
    if !status.success() {
        qp_bail!(
            "failed to compile `{}`",
            unit.root_package().package().manifest().name()
        )
    }
    Ok(())
}

/// Helper for [`compile_unit`].
fn compile_unit_impl(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &ProfileLayout,
    bcx: &BuildContext<'_, '_>,
) -> QuackResult<ExitStatus> {
    let packages = collect_packages(unit, graph);
    let task = create_task(unit, graph, layout);
    let manifest = multipackage_schema::MultiPackage {
        packages,
        tasks: vec![task],
    };
    let unit_layout = layout.for_dependency(&unit.unique_name());
    let profile = &bcx.profile;
    let mut builder = Duckc::new(bcx.pcx.ctx()).process_builder();
    builder
        .set_subcommand(DuckcSubcommand::CompilePackages)
        .set_manifest_path(&unit_layout.dependency_json_path())
        .set_artifacts_dir(&unit_layout.compiler_artifacts())
        .set_c_std(profile.c_std)
        .set_opt_level(profile.opt_level)
        .set_incremental(profile.incremental);
    let _lock = unit_layout.acquire_lock(bcx.pcx.ctx())?;
    let locked_manifest_file = unit_layout
        .dependency_json(bcx.pcx.ctx())
        .context("failed to open manifest.json")?;
    write_manifest(manifest, &locked_manifest_file)?;
    // Ensure we flush, by dropping the inner `File`.
    drop(locked_manifest_file);
    bcx.pcx.ctx().console().info(format!(
        "compiling `{}`...",
        unit.root_package().package().manifest().name()
    ))?;
    bcx.pcx
        .ctx()
        .console()
        .info_verbose(format!("Running `{}`", builder))?;
    builder.execute()
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
