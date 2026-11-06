// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various functionalities for interacting with external libraries specified in the manifest.

use std::collections::HashMap;
use std::convert::Infallible;
use std::env::consts::{DLL_PREFIX, DLL_SUFFIX};
use std::ops::ControlFlow;
use std::path::{Path, PathBuf};
use std::process::Command;

use tracing::debug;

use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph, UnitGraphNode};
use crate::quackpack::core::compile::unit::graph_visitor::GraphVisitor;
use crate::quackpack::core::compile::unit::{STATIC_LIB_SUFFIX, Unit};
use crate::{QuackResult, StrId, qp_bail};

#[derive(Debug)]
/// Marker struct indicating that we have found an external library.
///
/// It may not make sense to have explicit [`links`](ExternalLibrariesFound::links) field, but
/// otherwise it's hard for the caller to get the library, since the [`Unit`] only stores a list.
///
/// This struct has a logic invariant that
/// ```rust,ignore (illustrative)
/// self.unit.package().manifest().build_options().external_libs.first() == Some(&self.links)
/// ```
/// but allows caller to get `links` without going through an [`Option`].
pub struct ExternalLibrariesFound {
    /// [`Unit`] which links against the library.
    pub unit: Unit,
    /// The said external library.
    pub links: StrId,
}

/// Check whether subtree rooted at `unit` links against external libraries.
///
/// This function returns _any_ [`Unit`] with external library, if there is one.
///
/// In case there are multiple such [`Unit`]s, it's not guaranteed which one is returned.
pub fn has_external_libraries(
    unit_graph_id: GraphNodeId,
    graph: &UnitGraph,
) -> Option<ExternalLibrariesFound> {
    struct ExternalLibsVisitor;

    impl GraphVisitor for ExternalLibsVisitor {
        type Break = ExternalLibrariesFound;

        fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
            let Some(unit) = node.as_unit() else {
                return ControlFlow::Continue(());
            };
            let external_libs = &unit.package().manifest().build_options().external_libs;
            match external_libs.first() {
                Some(links) => ControlFlow::Break(ExternalLibrariesFound {
                    unit: unit.clone(),
                    links: *links,
                }),
                None => ControlFlow::Continue(()),
            }
        }
    }
    graph
        .node_for(unit_graph_id)
        .accept(&mut ExternalLibsVisitor {}, graph)
}

/// Validate external libraries against which we are linking.
/// Right now this checks for duplicates of the same library.
pub fn validate_external_libraries(
    unit_graph_id: GraphNodeId,
    graph: &UnitGraph,
) -> QuackResult<()> {
    #[derive(Default)]
    /// Map external lib -> Unit linking with it.
    struct DuplicateLibsVisitor(HashMap<StrId, Unit>);

    impl GraphVisitor for DuplicateLibsVisitor {
        /// (Dup A, Dup B, What)
        type Break = (Unit, Unit, StrId);

        fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
            let Some(unit) = node.as_unit() else {
                return ControlFlow::Continue(());
            };
            for &links in &unit.package().manifest().build_options().external_libs {
                let maybe_previous = self.0.insert(links, unit.clone());
                // Not a duplicate; the same package listing a library twice is harmless.
                let Some(previous) = maybe_previous.filter(|previous| previous != unit) else {
                    continue;
                };
                return ControlFlow::Break((previous, unit.clone(), links));
            }
            ControlFlow::Continue(())
        }
    }
    let unit = graph.node_for(unit_graph_id);
    let Some((dup_a, dup_b, links)) = unit.accept(&mut DuplicateLibsVisitor::default(), graph)
    else {
        return Ok(());
    };
    qp_bail!(
        "two different packages {} and {} both link against external library `{links}`",
        dup_a.pkg_descriptive_name(),
        dup_b.pkg_descriptive_name()
    )
}

/// Gather the linker arguments for all external libraries of the subgraph rooted at a given node
/// in the [`UnitGraph`]. See [`resolve_external_library`] for how a single library is resolved.
pub fn gather_external_libraries(unit_graph_id: GraphNodeId, graph: &UnitGraph) -> Vec<String> {
    #[derive(Default)]
    struct ExternalLibsCollector(Vec<String>);

    impl GraphVisitor for ExternalLibsCollector {
        type Break = Infallible;

        fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
            let Some(unit) = node.as_unit() else {
                return ControlFlow::Continue(());
            };
            let build_options = unit.package().manifest().build_options();
            if build_options.external_libs.is_empty() {
                return ControlFlow::Continue(());
            }
            let root = unit.package().root();
            let search_dirs = build_options
                .external_lib_paths
                .iter()
                .map(|dir| root.join(dir))
                .collect::<Vec<_>>();
            for &lib in &build_options.external_libs {
                self.0.extend(resolve_external_library(lib, &search_dirs));
            }
            ControlFlow::Continue(())
        }
    }
    let mut visitor = ExternalLibsCollector::default();
    let node = graph.node_for(unit_graph_id);
    node.accept(&mut visitor, graph);
    visitor.0
}

/// File names under which the native linker can find the library `name`, in order of preference.
///
/// The static archive comes first, so the produced binary does not need the library at runtime.
fn native_library_file_names(name: StrId) -> [String; 2] {
    [
        format!("lib{name}{STATIC_LIB_SUFFIX}"),
        format!("{DLL_PREFIX}{name}{DLL_SUFFIX}"),
    ]
}

/// Ask `pkg-config` for the directory holding the library `name`.
///
/// Returns [`None`] if `pkg-config` is not installed or does not know `name`.
fn pkg_config_lib_dir(name: StrId) -> Option<PathBuf> {
    let output = Command::new("pkg-config")
        .arg("--variable=libdir")
        .arg(name.as_str())
        .output()
        .inspect_err(|err| debug!(%name, %err, "failed to run `pkg-config`"))
        .ok()?;
    if !output.status.success() {
        debug!(%name, "`pkg-config` does not know the library");
        return None;
    }
    let dir = String::from_utf8(output.stdout).ok()?;
    let dir = dir.trim();
    (!dir.is_empty()).then(|| PathBuf::from(dir))
}

/// Resolve the external library `name` to the arguments for the native linker.
///
/// The library is searched for, as `lib<name>.a` first and then as a shared library, in:
/// 1. `search_dirs` (the package's `external-lib-paths`), in order,
/// 2. the `libdir` that `pkg-config` reports for `name`.
///
/// A found library is passed by its path; a shared one also gets an `rpath` entry, so the binary
/// finds it at runtime. When the library is not found anywhere, this falls back to `-l<name>` and
/// leaves the search to the linker's default directories.
pub fn resolve_external_library(name: StrId, search_dirs: &[PathBuf]) -> Vec<String> {
    let find_in = |dir: &Path| {
        native_library_file_names(name)
            .into_iter()
            .map(|file_name| dir.join(file_name))
            .find(|path| path.is_file())
    };
    let found = search_dirs
        .iter()
        .find_map(|dir| find_in(dir))
        .or_else(|| pkg_config_lib_dir(name).and_then(|dir| find_in(&dir)));
    debug!(%name, ?search_dirs, ?found, "resolved external library");
    let Some(path) = found else {
        return vec![format!("-l{name}")];
    };
    let is_static = path
        .file_name()
        .is_some_and(|file_name| file_name.to_string_lossy().ends_with(STATIC_LIB_SUFFIX));
    let mut args = vec![path.display().to_string()];
    if !is_static
        && cfg!(unix)
        && let Some(dir) = path.parent()
    {
        args.push(format!("-Wl,-rpath,{}", dir.display()));
    }
    args
}

#[cfg(test)]
mod tests {
    use std::fs;

    use super::*;

    #[test]
    fn test_resolve_prefers_first_search_dir_and_static_archive() {
        let first = tempfile::tempdir().unwrap();
        let second = tempfile::tempdir().unwrap();
        let [static_name, shared_name] = native_library_file_names("duckfoo".into());
        fs::write(first.path().join(&shared_name), "").unwrap();
        fs::write(first.path().join(&static_name), "").unwrap();
        fs::write(second.path().join(&static_name), "").unwrap();

        let args = resolve_external_library(
            "duckfoo".into(),
            &[first.path().to_path_buf(), second.path().to_path_buf()],
        );
        assert_eq!(args, [first.path().join(static_name).display().to_string()]);
    }

    #[test]
    fn test_resolve_shared_library_adds_rpath() {
        let dir = tempfile::tempdir().unwrap();
        let [_, shared_name] = native_library_file_names("duckfoo".into());
        fs::write(dir.path().join(&shared_name), "").unwrap();

        let args = resolve_external_library("duckfoo".into(), &[dir.path().to_path_buf()]);
        let mut expected = vec![dir.path().join(shared_name).display().to_string()];
        if cfg!(unix) {
            expected.push(format!("-Wl,-rpath,{}", dir.path().display()));
        }
        assert_eq!(args, expected);
    }

    #[test]
    fn test_resolve_missing_library_falls_back_to_linker_search() {
        let dir = tempfile::tempdir().unwrap();
        let args = resolve_external_library(
            "duck-surely-not-installed".into(),
            &[dir.path().to_path_buf()],
        );
        assert_eq!(args, ["-lduck-surely-not-installed"]);
    }
}
