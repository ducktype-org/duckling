//! [`Executor`] takes a [`UnitGraph`] and compiles it, according to its strategy.

use std::fmt::{self, Debug};
use std::io::Write;
use std::path::PathBuf;
use std::process::ExitStatus;

pub mod debug_executor;
pub mod dvm_executor;

#[cfg(test)]
mod tests;

use itertools::Itertools;
use tracing::debug;

use self::debug_executor::DebugExecutor;
use self::dvm_executor::DvmExecutor;
use super::BuildContext;
use super::artifacts_layout::{DependencyLayout, ProfileLayout};
use super::duckc::process_builder::DuckcSubcommand;
use super::duckc::{Duckc, multipackage_schema, process_builder};
use super::profiles::Profile;
use super::unit::Unit;
use super::unit::graph::UnitGraph;
use crate::quackpack::core::compile::unit::ArtifactsType;
use crate::quackpack::core::compile::unit::unit_visitor::UnitVisitor;
use crate::util::file_locks::LockedFile;
use crate::{QuackResult, QuackResultContext, qp_bail};

/// A generic duckc driver.
pub trait Executor: Debug {
    /// Try to compile the given [`UnitGraph`].
    fn compile(&self, graph: UnitGraph, bcx: &BuildContext<'_, '_>) -> QuackResult<ExecutorOutput>;
}

#[derive(Debug)]
/// Output of [`Executor::compile`].
pub struct ExecutorOutput {
    /// Root [`Unit`] and path to its output.
    pub root: (Unit, PathBuf),
}

impl BuildContext<'_, '_> {
    /// Get an appropriate executor.
    pub fn executor(&self) -> Box<dyn Executor> {
        if self.profile.dvm_bytecode {
            return Box::new(DvmExecutor);
        }
        Box::new(DebugExecutor)
    }
}

/// Collect this [`Unit`] and all its dependencies (direct and transitive), as a vector of
/// [`multipackage_schema::Package`].
///
/// Dependencies appearing in cycles are also included.
///
/// Each dependency is present exactly once.
pub(crate) fn collect_packages(
    unit: &Unit,
    graph: &UnitGraph,
) -> QuackResult<Vec<multipackage_schema::Package>> {
    struct PackageVisitor<'graph> {
        graph: &'graph UnitGraph,
        packages: Vec<multipackage_schema::Package>,
    }

    impl UnitVisitor for PackageVisitor<'_> {
        fn visit(&mut self, unit: &Unit) -> QuackResult<()> {
            self.packages
                .push(unit.multipackage_schema_package(self.graph)?);
            Ok(())
        }
    }
    let mut visitor = PackageVisitor {
        graph,
        packages: vec![],
    };
    unit.accept(&mut visitor, graph)?;
    Ok(visitor.packages)
}

/// Get linker options appropriate for the given `unit`.
///
/// Right now, this:
/// 1. returns `None` if there are no dependencies,
/// 2. creates a [`multipackage_schema::LinkerOptions::RawLinkerArgs`] only for
///    [`ArtifactsType::IsADependencyArtifact`] dependencies (which _should_ be only `.a` files).
pub(crate) fn get_linker_options(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &impl ProfileLayout,
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
    debug!(args = ?string, "raw linker args");
    if string.is_empty() {
        return Ok(None);
    }
    Ok(Some(multipackage_schema::LinkerOptions::RawLinkerArgs(
        string,
    )))
}

/// Collect _all_ (including `.a`!) outputs of dependencies (direct and transitive) of this `unit`.
pub(crate) fn get_deps_outputs<T: ProfileLayout>(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &T,
) -> QuackResult<Vec<(Unit, PathBuf)>> {
    struct UnitOutputVisitor<'a, U: ProfileLayout> {
        graph: &'a UnitGraph,
        layout: &'a U,
        root: &'a Unit,
        outputs: Vec<(Unit, PathBuf)>,
    }

    impl<U: ProfileLayout> UnitVisitor for UnitOutputVisitor<'_, U> {
        fn visit(&mut self, unit: &Unit) -> QuackResult<()> {
            if self.root != unit {
                self.outputs
                    .push((unit.clone(), unit_output(unit, self.graph, self.layout)?))
            }
            Ok(())
        }
    }
    let mut visitor = UnitOutputVisitor {
        graph,
        layout,
        root: unit,
        outputs: vec![],
    };
    unit.accept(&mut visitor, graph)?;
    Ok(visitor.outputs)
}

/// Get a path to the output artifact of this `unit`.
pub(crate) fn unit_output(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &impl ProfileLayout,
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
pub(crate) fn write_schema(
    schema: multipackage_schema::MultiPackage,
    locked_manifest_file: &LockedFile,
) -> QuackResult<()> {
    let manifest_json = serde_json::to_string_pretty(&schema).with_context_internal(|| {
        format!("failed to convert manifest into a JSON string: {schema:#?}")
    })?;
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
    layout: &impl DependencyLayout,
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
pub(crate) fn compile_single_unit_with_schema(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &impl ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    schema: multipackage_schema::MultiPackage,
) -> QuackResult<()> {
    let name = unit.root_package().package().name();
    let status = (|| {
        let unit_layout = layout.for_dependency(unit, graph)?;
        let builder = finished_builder_for_layout_and_profile(bcx, &unit_layout, &bcx.profile);
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
pub(crate) fn compile_single_unit_with_tasks(
    unit: &Unit,
    graph: &UnitGraph,
    layout: &impl ProfileLayout,
    bcx: &BuildContext<'_, '_>,
    tasks: Vec<multipackage_schema::Task>,
) -> QuackResult<()> {
    let packages = collect_packages(unit, graph)?;
    let schema = multipackage_schema::MultiPackage { packages, tasks };
    compile_single_unit_with_schema(unit, graph, layout, bcx, schema)
}
