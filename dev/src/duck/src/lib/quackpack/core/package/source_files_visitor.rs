// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Utilities for visiting all Duckling source files inside [`AnyPackage`].

use std::collections::{HashSet, VecDeque};
use std::ops::ControlFlow;
use std::path::{Path, PathBuf};

use crate::QuackResult;
use crate::quackpack::core::AnyPackage;
use crate::quackpack::core::script::Script;
use crate::quackpack::util::dir_iterator::create_dir_iterator;

/// A visitor of [`Path`].
///
/// Its implementors can be used by [`AnyPackage::try_visit_all_source_files`].
pub trait TryFileVisitor {
    /// Value reported by visitor if it stops visiting, for any reason.
    type Break;

    /// The role of this function is to gather information from the currently visited [`UnitGraphNode`].
    fn visit(&mut self, path: &Path) -> QuackResult<ControlFlow<Self::Break>>;
}

impl<V: TryFileVisitor + ?Sized> TryFileVisitor for &mut V {
    type Break = V::Break;
    fn visit(&mut self, unit: &Path) -> QuackResult<ControlFlow<Self::Break>> {
        (*self).visit(unit)
    }
}

impl<V: TryFileVisitor + ?Sized> TryFileVisitor for Box<V> {
    type Break = V::Break;
    fn visit(&mut self, unit: &Path) -> QuackResult<ControlFlow<Self::Break>> {
        (**self).visit(unit)
    }
}

impl AnyPackage {
    /// Visit all Duckling source files inside an [`AnyPackage`].
    ///
    /// If visitor returns an `Err(e)`, we short circuit to `Err(e)`
    ///
    /// If it returns `Ok(ControlFlow::Break(b))`, we short circuit to `Ok(Some(b))`.
    ///
    /// Otherwise (no errors + no breaks), we return `Ok(None)`.
    pub fn try_visit_all_source_files<V: TryFileVisitor>(
        &self,
        visitor: &mut V,
    ) -> QuackResult<Option<V::Break>> {
        // Path inside which all files are located.
        let start_file = match self {
            AnyPackage::Package(package) => package.source_directory().expect("cannot be global"),
            AnyPackage::Script(script) => match script {
                Script::Standalone(_) => script.script_path(),
                Script::Associated(associated) => {
                    if associated.package().is_global() {
                        script.script_path()
                    } else {
                        // Scripts with associated not global package lie in that package.
                        associated
                            .package()
                            .source_directory()
                            .expect("cannot be global")
                    }
                }
            },
        };
        let mut visited_paths = HashSet::new();
        let mut paths_to_visit: VecDeque<PathBuf> = [start_file.to_path_buf()].into();
        while let Some(path) = paths_to_visit.pop_front() {
            if visited_paths.contains(&path) {
                // We already visited this path, skip.
                continue;
            }
            if path.is_file() {
                // If a file, visit it.
                if let ControlFlow::Break(b) = visitor.visit(&path)? {
                    return Ok(Some(b));
                }
                visited_paths.insert(path);
            } else {
                // If a directory, mark all its entries as to be visited.
                for entry in create_dir_iterator(&path)? {
                    let entry = entry?;
                    paths_to_visit.push_back(entry.path());
                }
            }
        }
        Ok(None)
    }
}
