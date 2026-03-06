use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::{
    QuackResult, qp_internal,
    quackpack::core::{compile::compiler_package::CompilerPackage, storage::freeze::FreezeDep},
};

pub mod creating_graph;
pub mod modifying_graph;

#[cfg(test)]
mod tests;

#[derive(Debug)]
pub struct AllPackages {
    packages: HashMap<FreezeDep, CompilerPackage>,
}

impl AllPackages {
    pub fn package(&self, name: FreezeDep) -> QuackResult<&CompilerPackage> {
        self.packages
            .get(&name)
            .ok_or_else(|| qp_internal!("missing `{}` in a map", name))
    }

    pub fn package_mut(&mut self, name: FreezeDep) -> QuackResult<&mut CompilerPackage> {
        self.packages
            .get_mut(&name)
            .ok_or_else(|| qp_internal!("missing `{}` in a map", name))
    }
}

#[derive(Debug)]
pub struct CompilerGraph {
    all_packages: AllPackages,
    graph: DependencyGraph,
}

#[derive(Debug, PartialEq, Eq)]
pub struct DependencyGraph {
    root: DependencyNode,
}

#[derive(Debug, PartialEq, Eq)]
pub struct DependencyNode {
    node: FreezeDep,
    dependencies: Vec<DependencyNode>,
}

impl DependencyGraph {
    pub fn root(&self) -> &DependencyNode {
        &self.root
    }

    pub fn bail_if_has_cycles(&self) -> QuackResult<()> {
        // NOTE: [`reverse_topo_sort_order`](Self::reverse_topo_sort_order) fails,
        // if it has encountered a cycle.
        self.reverse_topo_sort_order().map(drop)
    }
}

impl DependencyNode {
    pub fn node(&self) -> FreezeDep {
        self.node
    }

    pub fn dependencies(&self) -> &[DependencyNode] {
        &self.dependencies
    }
}

impl CompilerGraph {
    pub fn package(&self, name: FreezeDep) -> QuackResult<&CompilerPackage> {
        self.all_packages.package(name)
    }

    pub fn package_mut(&mut self, name: FreezeDep) -> QuackResult<&mut CompilerPackage> {
        self.all_packages.package_mut(name)
    }

    pub fn graph(&self) -> &DependencyGraph {
        &self.graph
    }
}
