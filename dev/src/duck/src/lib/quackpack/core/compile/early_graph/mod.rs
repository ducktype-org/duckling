//! Dependency DAG and dictionary with all parsed [`CompilerPackage`]s.

use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::quackpack::core::compile::MISSING_DEPENDENCY_IN_GRAPH_MESSAGE;
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::identity::Identity;
use crate::{QuackError, QuackResult};

pub mod creating_graph;
pub mod modifying_graph;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// Dictionary [`Identity`] -> [`CompilerPackage`].
///
/// *Should* contain root and all dependencies listed in a freezefile.
pub struct PackagesSet {
    inner: HashMap<Identity, CompilerPackage>,
}

impl PackagesSet {
    /// Get [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &Identity) -> &CompilerPackage {
        self.inner
            .get(name)
            .expect(MISSING_DEPENDENCY_IN_GRAPH_MESSAGE)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &Identity) -> &mut CompilerPackage {
        self.inner
            .get_mut(name)
            .expect(MISSING_DEPENDENCY_IN_GRAPH_MESSAGE)
    }
}

#[derive(Debug)]
/// Dependency graph ([`DependencyGraph`]) + packages cache ([`PackagesSet`]).
pub struct EarlyGraph {
    packages: PackagesSet,
    graph: DependencyGraph,
}

#[derive(Debug, PartialEq, Eq)]
/// Graph representing dependency-dependant relations.
pub struct DependencyGraph {
    root: Identity,
    graph: HashMap<Identity, DependencyNode>,
}

#[derive(Debug, PartialEq, Eq, Hash, Default)]
/// Single dependency node.
///
/// Contains dependencies of this node.
pub struct DependencyNode {
    dependencies: Vec<Identity>,
}

impl DependencyGraph {
    /// Get the root package of this graph.
    pub fn root(&self) -> Identity {
        self.root
    }

    /// Get the [`DependencyNode`] for the given package.
    pub fn dependencies_for_package(&self, package: &Identity) -> &DependencyNode {
        self.graph
            .get(package)
            .expect(MISSING_DEPENDENCY_IN_GRAPH_MESSAGE)
    }

    /// Get the iterator over all entries in this graph.
    pub fn iter(&self) -> impl Iterator<Item = (&Identity, &DependencyNode)> {
        self.graph.iter()
    }

    /// Get the iterator over all keys in this graph.
    pub fn keys(&self) -> impl Iterator<Item = &Identity> {
        self.graph.keys()
    }
}

impl DependencyNode {
    /// Get dependencies of this node.
    pub fn dependencies(&self) -> &[Identity] {
        &self.dependencies
    }

    /// Same as [`dependencies`](Self::dependencies), but returns a mutable vector.
    pub fn dependencies_mut(&mut self) -> &mut Vec<Identity> {
        &mut self.dependencies
    }
}

impl EarlyGraph {
    /// Get the [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &Identity) -> &CompilerPackage {
        self.packages.package(name)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &Identity) -> &mut CompilerPackage {
        self.packages.package_mut(name)
    }

    /// Get the underlying [`DependencyGraph`].
    pub fn graph(&self) -> &DependencyGraph {
        &self.graph
    }
}
