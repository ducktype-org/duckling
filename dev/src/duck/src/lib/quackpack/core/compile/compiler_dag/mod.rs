//! Dependency DAG and dictionary with all parsed [`CompilerPackage`]s.

use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::{
    QuackResult, QuackResultContext,
    quackpack::core::{compile::compiler_package::CompilerPackage, storage::freeze::FreezeDep},
};

pub mod creating_dag;
pub mod modifying_dag;

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// Dictionary [`FreezeDep`] -> [`CompilerPackage`].
///
/// *Should* contain root and all dependencies listed in a freezefile.
pub struct AllPackages {
    packages: HashMap<FreezeDep, CompilerPackage>,
}

impl AllPackages {
    /// Get [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &FreezeDep) -> QuackResult<&CompilerPackage> {
        self.packages
            .get(name)
            .with_context_internal(|| format!("missing `{}` in a map", name))
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &FreezeDep) -> QuackResult<&mut CompilerPackage> {
        self.packages
            .get_mut(name)
            .with_context_internal(|| format!("missing `{}` in a map", name))
    }
}

#[derive(Debug)]
/// Dependency DAG ([`DependencyDag`]) + packages cache ([`AllPackages`]).
pub struct CompilerDag {
    all_packages: AllPackages,
    dag: DependencyDag,
}

#[derive(Debug, PartialEq, Eq)]
/// DAG representing dependency-dependant relations.
pub struct DependencyDag {
    root: FreezeDep,
    dag: HashMap<FreezeDep, DependencyNode>,
}

#[derive(Debug, PartialEq, Eq, Hash, Default)]
/// Single dependency node.
///
/// Contains dependencies of this node.
pub struct DependencyNode {
    dependencies: Vec<FreezeDep>,
}

impl DependencyDag {
    /// Get the root package of this DAG.
    pub fn root(&self) -> FreezeDep {
        self.root
    }

    /// Get the [`DependencyNode`] for the given package.
    pub fn dependencies_for_package(&self, package: &FreezeDep) -> QuackResult<&DependencyNode> {
        self.dag
            .get(package)
            .with_context_internal(|| format!("missing `{}` in a dag", package))
    }
}

impl DependencyNode {
    /// Get dependencies of this node.
    pub fn dependencies(&self) -> &[FreezeDep] {
        &self.dependencies
    }

    /// Same as [`dependencies`](Self::dependencies), but returns a mutable vector.
    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezeDep> {
        &mut self.dependencies
    }
}

impl CompilerDag {
    /// Get the [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &FreezeDep) -> QuackResult<&CompilerPackage> {
        self.all_packages.package(name)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &FreezeDep) -> QuackResult<&mut CompilerPackage> {
        self.all_packages.package_mut(name)
    }

    /// Get the underlying [`DependencyDag`].
    pub fn dag(&self) -> &DependencyDag {
        &self.dag
    }
}
