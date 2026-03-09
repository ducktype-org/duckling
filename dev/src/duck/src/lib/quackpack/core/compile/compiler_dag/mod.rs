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
pub struct AllPackages {
    packages: HashMap<FreezeDep, CompilerPackage>,
}

impl AllPackages {
    pub fn package(&self, name: &FreezeDep) -> QuackResult<&CompilerPackage> {
        self.packages
            .get(name)
            .with_context_internal(|| format!("missing `{}` in a map", name))
    }

    pub fn package_mut(&mut self, name: &FreezeDep) -> QuackResult<&mut CompilerPackage> {
        self.packages
            .get_mut(name)
            .with_context_internal(|| format!("missing `{}` in a map", name))
    }
}

#[derive(Debug)]
pub struct CompilerDag {
    all_packages: AllPackages,
    dag: DependencyDag,
}

#[derive(Debug, PartialEq, Eq)]
pub struct DependencyDag {
    root: FreezeDep,
    dag: HashMap<FreezeDep, DependencyNode>,
}

#[derive(Debug, PartialEq, Eq, Hash, Default)]
pub struct DependencyNode {
    dependencies: Vec<FreezeDep>,
}

impl DependencyDag {
    pub fn root(&self) -> FreezeDep {
        self.root
    }

    pub fn dependencies_for_package(&self, package: &FreezeDep) -> QuackResult<&DependencyNode> {
        self.dag
            .get(package)
            .with_context_internal(|| format!("missing `{}` in a dag", package))
    }
}

impl DependencyNode {
    pub fn dependencies(&self) -> &[FreezeDep] {
        &self.dependencies
    }

    pub fn dependencies_mut(&mut self) -> &mut Vec<FreezeDep> {
        &mut self.dependencies
    }
}

impl CompilerDag {
    pub fn package(&self, name: &FreezeDep) -> QuackResult<&CompilerPackage> {
        self.all_packages.package(name)
    }

    pub fn package_mut(&mut self, name: &FreezeDep) -> QuackResult<&mut CompilerPackage> {
        self.all_packages.package_mut(name)
    }

    pub fn dag(&self) -> &DependencyDag {
        &self.dag
    }
}
