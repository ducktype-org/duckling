//! [`Executor`] takes a [`UnitGraph`] and compiles it, according to its strategy.

use std::fmt::{self, Debug};
use std::io::Write;
use std::ops::ControlFlow;
use std::path::PathBuf;
use std::process::ExitStatus;

pub mod default_unit_compiler;
pub mod dvm_unit_compiler;

#[cfg(test)]
mod tests;

use itertools::Itertools;
use tracing::{debug, instrument};

use self::default_unit_compiler::DefaultUnitCompiler;
use self::dvm_unit_compiler::DvmUnitCompiler;
use super::BuildContext;
use super::artifacts_layout::shared::SharedArtifactsLayout;
use super::artifacts_layout::standard::StandardArtifactsLayout;
use super::artifacts_layout::{ArtifactsLayout, DependencyLayout, ProfileLayout};
use super::duckc::process_builder::DuckcSubcommand;
use super::duckc::{Duckc, multipackage_schema, process_builder};
use super::profiles::Profile;
use super::unit::graph::UnitGraph;
use super::unit::unit_visitor::TryUnitVisitor;
use super::unit::{ArtifactsType, Unit};
use crate::util::file_locks::LockedFile;
use crate::{QuackError, QuackResult, QuackResultContext, qp_bail};

/// A generic duckc driver.
pub trait UnitCompiler: Debug {
    /// Try to compile the given [`UnitGraph`].
    #[instrument(skip_all)]
    fn compile(&self, graph: UnitGraph, bcx: &BuildContext<'_, '_>) -> QuackResult<ExecutorOutput> {
        self.pre_compilation(&graph, bcx);
        let artifacts_layout = bcx.artifacts_layout(&graph);
        let profile_layout = artifacts_layout.for_profile(bcx.profile);
        self.compile_all_needed_units(&graph, &*profile_layout, bcx)?;
        self.get_executor_output(&graph, &*profile_layout, bcx)
    }

    /// Callback invoked at the very start of [`compile`](Self::compile).
    fn pre_compilation(&self, graph: &UnitGraph, bcx: &BuildContext<'_, '_>);

    /// Create tasks which will be used for compiling the given [`Unit`].
    fn create_tasks(
        &self,
        unit: &Unit,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<Vec<multipackage_schema::Task>>;

    /// Get the list of [`Unit]`s to compile.
    fn units_to_compile<'a>(
        &self,
        graph: &'a UnitGraph,
        bcx: &BuildContext<'_, '_>,
    ) -> Vec<&'a Unit>;

    #[instrument(skip_all)]
    /// Compile a single [`Unit`].
    /// May panic, if this [`Unit`] is not in the [`units_to_compile`](Self::units_to_compile) list.
    fn compile_unit(
        &self,
        unit: &Unit,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<()> {
        let tasks = self.create_tasks(unit, graph, layout, bcx)?;
        compile_single_unit_with_tasks(unit, graph, layout, bcx, tasks)
    }

    #[instrument(skip_all)]
    /// Compile all [`Unit`]s from the [`units_to_compile`](Self::units_to_compile) list.
    fn compile_all_needed_units(
        &self,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<()> {
        for unit in self.units_to_compile(graph, bcx) {
            self.compile_unit(unit, graph, layout, bcx)?;
        }
        Ok(())
    }

    #[instrument(skip_all)]
    /// Get the output of this [`Executor`].
    fn get_executor_output(
        &self,
        graph: &UnitGraph,
        layout: &dyn ProfileLayout,
        bcx: &BuildContext<'_, '_>,
    ) -> QuackResult<ExecutorOutput> {
        let _ = bcx;
        let root = graph.root_unit();
        let path = unit_output(root, graph, layout)?;
        Ok(ExecutorOutput {
            root: (root.clone(), path),
        })
    }
}

#[derive(Debug)]
/// Output of [`Executor::compile`].
pub struct ExecutorOutput {
    /// Root [`Unit`] and path to its output.
    pub root: (Unit, PathBuf),
}

impl BuildContext<'_, '_> {
    /// Get an appropriate executor.
    pub fn unit_compiler(&self) -> Box<dyn UnitCompiler> {
        if self.profile.dvm_bytecode {
            debug!("returning DvmUnitCompiler");
            return Box::new(DvmUnitCompiler);
        }
        debug!("returning DefaultUnitCompiler");
        Box::new(DefaultUnitCompiler)
    }

    /// Get an appropriate layout implementation.
    pub fn artifacts_layout(&self, graph: &UnitGraph) -> Box<dyn ArtifactsLayout> {
        let root_package = graph.root_unit().root_package().package().get_package();
        let root_package_artifacts = root_package.artifacts_directory().to_path_buf();
        debug!(shared = %self.shared);
        if self.shared {
            Box::new(SharedArtifactsLayout::new(root_package_artifacts))
        } else {
            Box::new(StandardArtifactsLayout::new(root_package_artifacts))
        }
    }
}

/// Collect this [`Unit`] and all its dependencies (direct and transitive), as a vector of
/// [`multipackage_schema::Package`].
///
/// Dependencies appearing in cycles are also included.
///
/// Each dependency is present exactly once.
#[instrument(skip_all)]
pub(crate) fn collect_packages(
    unit: &Unit,
    graph: &UnitGraph,
) -> QuackResult<Vec<multipackage_schema::Package>> {
    struct PackageVisitor<'graph> {
        graph: &'graph UnitGraph,
        packages: Vec<multipackage_schema::Package>,
    }

    impl TryUnitVisitor for PackageVisitor<'_> {
        type Err = QuackError;

        type Break = ();

        fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err> {
            self.packages
                .push(unit.multipackage_schema_package(self.graph)?);
            Ok(ControlFlow::Continue(()))
        }
    }
    let mut visitor = PackageVisitor {
        graph,
        packages: vec![],
    };
    unit.try_accept(&mut visitor, graph)?;
    Ok(visitor.packages)
}

/// Get linker options appropriate for the given `unit`.
///
/// Right now, this:
/// 1. returns `None` if there are no dependencies,
/// 2. creates a [`multipackage_schema::LinkerOptions::RawLinkerArgs`] only for
///    [`ArtifactsType::IsADependencyArtifact`] dependencies (which _should_ be only `.a` files).
#[instrument(skip_all)]
pub(crate) fn get_linker_options(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<Option<multipackage_schema::LinkerOptions>> {
    let outputs = get_deps_outputs(unit, graph, layout)?;
    if outputs.is_empty() {
        return Ok(None);
    }
    let string = outputs
        .into_iter()
        .filter_map(|(unit, output)| {
            if unit.artifacts_type() == ArtifactsType::IsADependencyArtifact {
                Some(output)
            } else {
                None
            }
        })
        .map(|output| output.display().to_string())
        .join(" ");
    debug!(args = %string, "raw linker args");
    if string.is_empty() {
        return Ok(None);
    }
    Ok(Some(multipackage_schema::LinkerOptions::RawLinkerArgs(
        string,
    )))
}

/// Collect _all_ (including `.a`!) outputs of dependencies (direct and transitive) of this `unit`.
#[instrument(skip_all)]
pub(crate) fn get_deps_outputs(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<Vec<(Unit, PathBuf)>> {
    struct UnitOutputVisitor<'a> {
        graph: &'a UnitGraph,
        layout: &'a dyn ProfileLayout,
        root: &'a Unit,
        outputs: Vec<(Unit, PathBuf)>,
    }

    impl TryUnitVisitor for UnitOutputVisitor<'_> {
        type Err = QuackError;

        type Break = ();

        fn try_visit(&mut self, unit: &Unit) -> Result<ControlFlow<Self::Break>, Self::Err> {
            if self.root != unit {
                self.outputs
                    .push((unit.clone(), unit_output(unit, self.graph, self.layout)?))
            }
            Ok(ControlFlow::Continue(()))
        }
    }
    let mut visitor = UnitOutputVisitor {
        graph,
        layout,
        root: unit,
        outputs: vec![],
    };
    unit.try_accept(&mut visitor, graph)?;
    Ok(visitor.outputs)
}

/// Get a path to the output artifact of this `unit`.
#[instrument(skip_all)]
pub(crate) fn unit_output(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
) -> QuackResult<PathBuf> {
    let out = if graph.is_root(unit) {
        layout.root_directory().join(unit.output_file_name())
    } else {
        let layout = layout.for_dependency(unit, graph)?;
        layout.root_directory().join(unit.output_file_name())
    };
    debug!(unit = ?unit, out = %out.display(), "generating output");
    Ok(out)
}

/// Write a manifest into a file.
#[instrument(skip_all)]
pub(crate) fn write_schema(
    schema: multipackage_schema::MultiPackage,
    locked_manifest_file: &LockedFile,
) -> QuackResult<()> {
    let manifest_json = serde_json::to_string_pretty(&schema)
        .context_internal("failed to convert manifest into a JSON string")?;
    locked_manifest_file.file().set_len(0).with_context(|| {
        format!(
            "failed to truncate `{}`",
            locked_manifest_file.path().display()
        )
    })?;
    locked_manifest_file
        .file()
        .write_all(manifest_json.as_bytes())
        .with_context(|| {
            format!(
                "failed to write manifest to `{}`",
                locked_manifest_file.path().display()
            )
        })?;
    Ok(())
}

/// Create a basic and reusable [`process_builder::DuckcProcessBuilder`].
pub(crate) fn finished_builder_for_layout_and_profile(
    bcx: &BuildContext<'_, '_>,
    layout: &dyn DependencyLayout,
    profile: &Profile,
) -> process_builder::DuckcProcessBuilder {
    let mut builder = Duckc::new(bcx.pcx.ctx()).process_builder();
    builder
        .set_subcommand(DuckcSubcommand::CompilePackages)
        .set_manifest_path(&layout.dependency_json_path())
        .set_artifacts_dir(&layout.compiler_artifacts())
        .set_c_std(profile.c_std)
        .set_opt_level(profile.opt_level)
        .set_incremental(profile.incremental)
        .set_workers_count(bcx.jobs);
    builder
}

/// Execute a builder, and print messages.
#[instrument(skip_all)]
pub(crate) fn compile_and_print(
    bcx: &BuildContext<'_, '_>,
    mut builder: process_builder::DuckcProcessBuilder,
    name: impl fmt::Display,
) -> QuackResult<ExitStatus> {
    bcx.pcx
        .ctx()
        .console()
        .info(format!("compiling `{name}`..."))?;
    bcx.pcx
        .ctx()
        .console()
        .info_verbose(format!("Running `{}`", builder))?;
    builder.execute()
}

/// Compile a single [`Unit`] with its finished schema.
#[instrument(skip_all)]
pub(crate) fn compile_single_unit_with_schema(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    schema: multipackage_schema::MultiPackage,
) -> QuackResult<()> {
    let name = unit.root_package().package().name();
    let status = (|| {
        let unit_layout = layout.for_dependency(unit, graph)?;
        let builder = finished_builder_for_layout_and_profile(bcx, &*unit_layout, &bcx.profile);
        let _lock = unit_layout.acquire_lock(bcx.pcx.ctx())?;
        let locked_manifest_file = unit_layout
            .dependency_json(bcx.pcx.ctx())
            .context("failed to open `deps.json`")?;
        write_schema(schema, &locked_manifest_file)?;
        // Ensure we flush, by dropping the inner `File`.
        drop(locked_manifest_file);
        compile_and_print(bcx, builder, name)
    })()
    .with_context(|| format!("failed to compile `{name}`"))?;
    if !status.success() {
        qp_bail!("failed to compile `{name}`")
    }
    Ok(())
}

/// Compile a single [`Unit`] with its finished tasks.
#[instrument(skip_all)]
pub(crate) fn compile_single_unit_with_tasks(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &dyn ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    tasks: Vec<multipackage_schema::Task>,
) -> QuackResult<()> {
    let packages = collect_packages(unit, graph)?;
    let schema = multipackage_schema::MultiPackage { packages, tasks };
    compile_single_unit_with_schema(unit, graph, layout, bcx, schema)
}
