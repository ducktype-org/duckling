// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! `repl` subcommand execution logic.
//!
//! The REPL gets the packages of a package context: the root package (unless it is the global
//! package, which has no sources) and its dependencies are importable from cells, while transitive
//! dependencies are only loaded.
use std::convert::Infallible;
use std::path::{Path, PathBuf};

use crate::quackpack::core::compile::duckc::Duckc;
use crate::quackpack::core::compile::duckc::multipackage_schema::{Dependency, ReplPackages};
use crate::quackpack::core::compile::early_graph::creating_graph::create_early_graph_from_bcx;
use crate::quackpack::core::compile::profiles::Profile;
use crate::quackpack::core::compile::unit::graph::{UnitGraph, lower_early_graph};
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::storage::{StorageSyncOptions, SyncOutput, sync};
use crate::quackpack::core::PackageContext;
use crate::{QuackResult, QuackResultContext, StrId};

#[derive(Debug)]
/// Options for starting a REPL session.
pub struct ReplOptions<'duck> {
    /// Package whose dependencies the session gets.
    pub pcx: PackageContext<'duck>,
    /// Artefact from [`StorageSyncOptions`].
    pub overwrite: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub frozen: bool,
    /// Artefact from [`StorageSyncOptions`].
    pub strict_errors: bool,
    /// Extra arguments passed to `duckc repl`.
    pub duckc_args: Vec<String>,
}

/// Sync the package, write the REPL packages manifest, and replace this process with `duckc repl`.
pub fn repl(options: ReplOptions<'_>) -> QuackResult<Infallible> {
    let manifest_path = write_repl_packages(&options)?;
    Duckc::new(options.pcx.ctx()).start_repl(Some(&manifest_path), &options.duckc_args)
}

/// Sync the package and write its REPL packages manifest, returning the manifest's path.
pub fn write_repl_packages(options: &ReplOptions<'_>) -> QuackResult<PathBuf> {
    let pcx = &options.pcx;
    let root_identity = pcx.package().as_a_local_identity()?;
    let SyncOutput {
        new_freeze,
        loaded_packages,
        sync_lock: _lock,
        new_venv: _,
        storage,
    } = sync(
        pcx,
        StorageSyncOptions {
            overwrite: options.overwrite,
            frozen: options.frozen,
            strict_errors: options.strict_errors,
        },
    )?;
    let profile = Profile::construct_profile("dev".into(), pcx.package().manifest().profiles())?;
    let bcx = BuildContext {
        pcx,
        root_identity,
        freeze: new_freeze,
        storage,
        used_features: vec![],
        profile,
        shared: false,
        jobs: 1,
    };
    let unit_graph = lower_early_graph(create_early_graph_from_bcx(&bcx, loaded_packages)?)?;
    let repl_packages = repl_packages(&unit_graph)?;

    let manifest_path = repl_manifest_path(pcx.package().artifacts_directory());
    write_json(&manifest_path, &repl_packages)?;
    Ok(manifest_path)
}

/// Build the [`ReplPackages`] for the root package of `graph`.
pub fn repl_packages(graph: &UnitGraph) -> QuackResult<ReplPackages> {
    let root = graph.root_unit().package_data();

    let mut packages = vec![];
    for package in graph.all_pkgs().values() {
        if package.package().src().is_none() {
            continue;
        }
        packages.push(package.multipackage_schema_package(graph)?);
    }
    packages.sort_by(|a, b| a.id.cmp(&b.id));

    let mut session_dependencies = vec![];
    if root.package().src().is_some() {
        session_dependencies.push(Dependency {
            id: root.unique_name().into(),
            alias: None,
        });
    }
    for (dep, alias) in root.deps_realization() {
        session_dependencies.push(Dependency {
            id: graph.package_data(*dep).unique_name().into(),
            alias: alias.map(|alias: StrId| alias.to_string()),
        });
    }

    Ok(ReplPackages {
        packages,
        session_dependencies,
    })
}

fn repl_manifest_path(artifacts_dir: &Path) -> PathBuf {
    artifacts_dir.join("repl").join("packages.json")
}

fn write_json(path: &Path, repl_packages: &ReplPackages) -> QuackResult<()> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)
            .with_context(|| format!("failed to create `{}`", parent.display()))?;
    }
    let content = serde_json::to_string_pretty(repl_packages)
        .context("failed to serialize the REPL packages manifest")?;
    std::fs::write(path, content).with_context(|| format!("failed to write `{}`", path.display()))
}
