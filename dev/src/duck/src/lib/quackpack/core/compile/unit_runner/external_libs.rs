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
use crate::quackpack::core::{ExternalLib, LinkKind};
use crate::{QuackResult, StrId, qp_bail, qp_err};

#[derive(Debug)]
/// Marker struct indicating that we have found an external library.
///
/// It may not make sense to have explicit [`lib`](ExternalLibrariesFound::lib) field, but
/// otherwise it's hard for the caller to get the library, since the [`Unit`] only stores a list.
///
/// This struct has a logic invariant that
/// ```rust,ignore (illustrative)
/// self.unit.package().manifest().build_options().external_libs.first().map(|lib| lib.name) == Some(self.lib)
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
                Some(lib) => ControlFlow::Break(ExternalLibrariesFound {
                    unit: unit.clone(),
                    lib: lib.name,
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
            for lib in &unit.package().manifest().build_options().external_libs {
                let lib = lib.name;
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

/// What kind of file an [`ExternalLibrary`] was found as, if it was found at all.
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
    /// How the package asked for the library to be linked.
    pub link_kind: LinkKind,
    /// [`Unit`] whose package lists the library, for diagnostics.
    pub unit: Unit,
    /// What kind of file the library was found as.
    pub kind: ExternalLibraryKind,
}

impl ExternalLibrary {
    /// The native linker arguments for this library.
    ///
    /// A found library is passed by its path; a shared one also gets an `rpath` entry, so the
    /// binary finds it at runtime. A dynamic library that was not found falls back to
    /// `-l<name>`, which leaves the search to the linker. A static library that was not found is
    /// an error, because the linker would pick the shared library instead.
    pub fn native_linker_args(&self) -> QuackResult<Vec<String>> {
        Ok(match &self.kind {
            ExternalLibraryKind::NotFound => match self.link_kind {
                LinkKind::Dynamic => vec![format!("-l{}", self.name)],
                LinkKind::Static => {
                    let mut err = qp_err!(
                        "package {} links `{}` statically, but `lib{}{STATIC_LIB_SUFFIX}` was not found",
                        self.unit.pkg_descriptive_name(),
                        self.name,
                        self.name,
                    );
                    err.add_hint(
                        "put the archive's directory in `metadata.external-lib-paths`, or link the library dynamically",
                    );
                    return Err(err);
                }
            },
            ExternalLibraryKind::Static { path } => vec![path.display().to_string()],
            ExternalLibraryKind::Shared { path } => {
                let mut args = vec![path.display().to_string()];
                if cfg!(unix)
                    && let Some(dir) = path.parent()
                {
                    args.push(format!("-Wl,-rpath,{}", dir.display()));
                }
                args
            }
        })
    }
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
                .extend(external_libs.iter().map(|&lib| ExternalLibrary {
                    name: lib.name,
                    link_kind: lib.kind,
                    unit: unit.clone(),
                    kind: resolve_external_library(lib, &search_dirs),
                }));
            ControlFlow::Continue(())
        }
    }
    let mut visitor = ExternalLibsCollector::default();
    let node = graph.node_for(unit_graph_id);
    node.accept(&mut visitor, graph);
    visitor.0
}

/// The file name under which the library `name` is searched for, when linked as `kind`.
fn native_library_file_name(name: StrId, kind: LinkKind) -> String {
    match kind {
        LinkKind::Dynamic => format!("{DLL_PREFIX}{name}{DLL_SUFFIX}"),
        LinkKind::Static => format!("lib{name}{STATIC_LIB_SUFFIX}"),
    }
}

/// Find the external library `lib` in `search_dirs`.
///
/// The library is searched for by file name only, in each of `search_dirs` in order: the shared
/// library if `lib` is linked dynamically, the static archive if it is linked statically. Nothing
/// else is consulted (no `pkg-config`).
pub fn resolve_external_library(lib: ExternalLib, search_dirs: &[PathBuf]) -> ExternalLibraryKind {
    let file_name = native_library_file_name(lib.name, lib.kind);
    let found = search_dirs
        .iter()
        .map(|dir| dir.join(&file_name))
        .find(|path| path.is_file());
    debug!(name = %lib.name, kind = ?lib.kind, ?search_dirs, ?found, "resolved external library");
    match (found, lib.kind) {
        (None, _) => ExternalLibraryKind::NotFound,
        (Some(path), LinkKind::Static) => ExternalLibraryKind::Static { path },
        (Some(path), LinkKind::Dynamic) => ExternalLibraryKind::Shared { path },
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::util::path_ops::touch;

    fn lib(kind: LinkKind) -> ExternalLib {
        ExternalLib {
            name: "duckfoo".into(),
            kind,
        }
    }

    /// Two search directories holding both kinds of `duckfoo`, in the first one only the given
    /// kinds.
    fn two_dirs(first_has: &[LinkKind]) -> [tempfile::TempDir; 2] {
        let dirs = [tempfile::tempdir().unwrap(), tempfile::tempdir().unwrap()];
        for &kind in first_has {
            touch(dirs[0].path().join(native_library_file_name("duckfoo".into(), kind))).unwrap();
        }
        for kind in [LinkKind::Dynamic, LinkKind::Static] {
            touch(dirs[1].path().join(native_library_file_name("duckfoo".into(), kind))).unwrap();
        }
        dirs
    }

    fn paths(dirs: &[tempfile::TempDir]) -> Vec<PathBuf> {
        dirs.iter().map(|dir| dir.path().to_path_buf()).collect()
    }

    #[test]
    fn test_dynamic_takes_the_shared_library_even_next_to_an_archive() {
        let dirs = two_dirs(&[LinkKind::Dynamic, LinkKind::Static]);
        let kind = resolve_external_library(lib(LinkKind::Dynamic), &paths(&dirs));
        assert_eq!(
            kind,
            ExternalLibraryKind::Shared {
                path: dirs[0]
                    .path()
                    .join(native_library_file_name("duckfoo".into(), LinkKind::Dynamic))
            }
        );
    }

    #[test]
    fn test_static_takes_the_archive_even_next_to_a_shared_library() {
        let dirs = two_dirs(&[LinkKind::Dynamic, LinkKind::Static]);
        let kind = resolve_external_library(lib(LinkKind::Static), &paths(&dirs));
        assert_eq!(
            kind,
            ExternalLibraryKind::Static {
                path: dirs[0]
                    .path()
                    .join(native_library_file_name("duckfoo".into(), LinkKind::Static))
            }
        );
    }

    #[test]
    fn test_each_kind_skips_directories_without_it() {
        let dirs = two_dirs(&[LinkKind::Static]);
        let kind = resolve_external_library(lib(LinkKind::Dynamic), &paths(&dirs));
        assert_eq!(
            kind,
            ExternalLibraryKind::Shared {
                path: dirs[1]
                    .path()
                    .join(native_library_file_name("duckfoo".into(), LinkKind::Dynamic))
            }
        );

        let dirs = two_dirs(&[LinkKind::Dynamic]);
        let kind = resolve_external_library(lib(LinkKind::Static), &paths(&dirs));
        assert_eq!(
            kind,
            ExternalLibraryKind::Static {
                path: dirs[1]
                    .path()
                    .join(native_library_file_name("duckfoo".into(), LinkKind::Static))
            }
        );
    }

    #[test]
    fn test_resolve_missing_library() {
        let dir = tempfile::tempdir().unwrap();
        for kind in [LinkKind::Dynamic, LinkKind::Static] {
            let missing = ExternalLib {
                name: "duck-surely-not-installed".into(),
                kind,
            };
            let found = resolve_external_library(missing, &[dir.path().to_path_buf()]);
            assert_eq!(found, ExternalLibraryKind::NotFound);
        }
    }
}
