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
use std::path::PathBuf;

use tracing::debug;

use crate::quackpack::core::compile::unit::graph::{GraphNodeId, UnitGraph, UnitGraphNode};
use crate::quackpack::core::compile::unit::graph_visitor::GraphVisitor;
use crate::quackpack::core::compile::unit::{STATIC_LIB_SUFFIX, Unit};
use crate::{QuackResult, StrId, qp_bail};

#[derive(Debug)]
/// Marker struct indicating that we have found an external library.
///
/// It may not make sense to have explicit [`lib`](ExternalLibrariesFound::lib) field, but
/// otherwise it's hard for the caller to get the library, since the [`Unit`] only stores a list.
///
/// This struct has a logic invariant that
/// ```rust,ignore (illustrative)
/// self.unit.package().manifest().build_options().external_libs.first() == Some(&self.lib)
/// ```
/// but allows caller to get `lib` without going through an [`Option`].
pub struct ExternalLibrariesFound {
    /// [`Unit`] which links against the library.
    pub unit: Unit,
    /// The said external library.
    pub lib: StrId,
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
                Some(&lib) => ControlFlow::Break(ExternalLibrariesFound {
                    unit: unit.clone(),
                    lib,
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
            for &lib in &unit.package().manifest().build_options().external_libs {
                let maybe_previous = self.0.insert(lib, unit.clone());
                // Duplicates are allowed in the same package.
                let Some(previous) = maybe_previous.filter(|previous| previous != unit) else {
                    continue;
                };
                return ControlFlow::Break((previous, unit.clone(), lib));
            }
            ControlFlow::Continue(())
        }
    }
    let unit = graph.node_for(unit_graph_id);
    let Some((dup_a, dup_b, lib)) = unit.accept(&mut DuplicateLibsVisitor::default(), graph) else {
        return Ok(());
    };
    qp_bail!(
        "two different packages {} and {} both link against external library `{lib}`",
        dup_a.pkg_descriptive_name(),
        dup_b.pkg_descriptive_name()
    )
}

/// Where an [`ExternalLibrary`] was found.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum ExternalLibraryKind {
    /// No file for the library is in any search directory.
    NotFound,
    /// A static archive (`lib<name>.a`).
    Static {
        /// Path to the archive.
        path: PathBuf,
    },
    /// A shared library (`lib<name>.so`, `.dylib` or `.dll`, depending on the host).
    Shared {
        /// Path to the shared library.
        path: PathBuf,
    },
}

/// An external library of a [`Unit`], resolved to a file (or not).
///
/// This holds no linker arguments on purpose: the caller (the native task, and later the DVM)
/// decides how to pass the library on.
#[derive(Clone, Debug)]
pub struct ExternalLibrary {
    /// The library's name, as written in `external-libs`.
    pub name: StrId,
    /// [`Unit`] whose package lists the library, for diagnostics.
    pub unit: Unit,
    /// Where the library was found.
    pub kind: ExternalLibraryKind,
}

/// Directories searched after a package's own `external-lib-paths`.
#[cfg(target_os = "linux")]
const DEFAULT_EXTERNAL_LIB_PATHS: &[&str] = &[
    "/usr/local/lib",
    "/usr/lib",
    "/lib",
    "/usr/lib64",
    "/lib64",
    #[cfg(target_arch = "x86_64")]
    "/usr/lib/x86_64-linux-gnu",
    #[cfg(target_arch = "x86_64")]
    "/lib/x86_64-linux-gnu",
    #[cfg(target_arch = "aarch64")]
    "/usr/lib/aarch64-linux-gnu",
    #[cfg(target_arch = "aarch64")]
    "/lib/aarch64-linux-gnu",
];
/// Directories searched after a package's own `external-lib-paths`.
#[cfg(target_os = "macos")]
const DEFAULT_EXTERNAL_LIB_PATHS: &[&str] = &["/usr/local/lib", "/opt/homebrew/lib", "/usr/lib"];
/// Directories searched after a package's own `external-lib-paths`.
#[cfg(not(any(target_os = "linux", target_os = "macos")))]
const DEFAULT_EXTERNAL_LIB_PATHS: &[&str] = &[];

/// The directories searched for the external libraries of `unit`, in order: the package's
/// `external-lib-paths` (relative ones joined to the package's root), then
/// [`DEFAULT_EXTERNAL_LIB_PATHS`].
fn search_dirs_for(unit: &Unit) -> Vec<PathBuf> {
    let root = unit.package().root();
    unit.package()
        .manifest()
        .build_options()
        .external_lib_paths
        .iter()
        // An absolute `dir` replaces `root` here, which is what we want.
        .map(|dir| root.join(dir))
        .chain(DEFAULT_EXTERNAL_LIB_PATHS.iter().map(PathBuf::from))
        .collect()
}

/// Gather all external libraries of the subgraph rooted at a given node in the [`UnitGraph`].
/// See [`resolve_external_library`] for how a single library is resolved.
///
/// This touches the filesystem, but it is only called once per build, for the root's task.
pub fn gather_external_libraries(
    unit_graph_id: GraphNodeId,
    graph: &UnitGraph,
) -> Vec<ExternalLibrary> {
    #[derive(Default)]
    struct ExternalLibsCollector(Vec<ExternalLibrary>);

    impl GraphVisitor for ExternalLibsCollector {
        type Break = Infallible;

        fn visit(&mut self, node: &UnitGraphNode) -> ControlFlow<Self::Break> {
            let Some(unit) = node.as_unit() else {
                return ControlFlow::Continue(());
            };
            let external_libs = &unit.package().manifest().build_options().external_libs;
            if external_libs.is_empty() {
                return ControlFlow::Continue(());
            }
            let search_dirs = search_dirs_for(unit);
            self.0
                .extend(external_libs.iter().map(|&name| ExternalLibrary {
                    name,
                    unit: unit.clone(),
                    kind: resolve_external_library(name, &search_dirs),
                }));
            ControlFlow::Continue(())
        }
    }
    let mut visitor = ExternalLibsCollector::default();
    let node = graph.node_for(unit_graph_id);
    node.accept(&mut visitor, graph);
    visitor.0
}

/// File names under which the library `name` is searched for, in order of preference.
///
/// The static archive comes first, so the produced binary does not need the library at runtime.
fn native_library_file_names(name: StrId) -> [String; 2] {
    [
        format!("lib{name}{STATIC_LIB_SUFFIX}"),
        format!("{DLL_PREFIX}{name}{DLL_SUFFIX}"),
    ]
}

/// Find the external library `name` in `search_dirs`.
///
/// The library is searched for by file name only, as `lib<name>.a` first and then as a shared
/// library, in each of `search_dirs` in order. Nothing else is consulted (no `pkg-config`).
pub fn resolve_external_library(name: StrId, search_dirs: &[PathBuf]) -> ExternalLibraryKind {
    let file_names = native_library_file_names(name);
    let found = search_dirs.iter().find_map(|dir| {
        file_names
            .iter()
            .map(|file_name| dir.join(file_name))
            .find(|path| path.is_file())
    });
    debug!(%name, ?search_dirs, ?found, "resolved external library");
    match found {
        None => ExternalLibraryKind::NotFound,
        Some(path) if path.ends_with(&file_names[0]) => ExternalLibraryKind::Static { path },
        Some(path) => ExternalLibraryKind::Shared { path },
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::util::path_ops::touch;

    #[test]
    fn test_resolve_prefers_first_search_dir_and_static_archive() {
        let first = tempfile::tempdir().unwrap();
        let second = tempfile::tempdir().unwrap();
        let [static_name, shared_name] = native_library_file_names("duckfoo".into());
        touch(first.path().join(&shared_name)).unwrap();
        touch(first.path().join(&static_name)).unwrap();
        touch(second.path().join(&static_name)).unwrap();

        let kind = resolve_external_library(
            "duckfoo".into(),
            &[first.path().to_path_buf(), second.path().to_path_buf()],
        );
        assert_eq!(
            kind,
            ExternalLibraryKind::Static {
                path: first.path().join(static_name)
            }
        );
    }

    #[test]
    fn test_resolve_shared_library() {
        let dir = tempfile::tempdir().unwrap();
        let [_, shared_name] = native_library_file_names("duckfoo".into());
        touch(dir.path().join(&shared_name)).unwrap();

        let kind = resolve_external_library("duckfoo".into(), &[dir.path().to_path_buf()]);
        assert_eq!(
            kind,
            ExternalLibraryKind::Shared {
                path: dir.path().join(shared_name)
            }
        );
    }

    #[test]
    fn test_resolve_missing_library() {
        let dir = tempfile::tempdir().unwrap();
        let kind = resolve_external_library(
            "duck-surely-not-installed".into(),
            &[dir.path().to_path_buf()],
        );
        assert_eq!(kind, ExternalLibraryKind::NotFound);
    }
}
