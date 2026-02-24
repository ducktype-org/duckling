use std::{
    collections::{HashMap, HashSet},
    sync::RwLock,
};

use itertools::Itertools;
use tracing::debug;

use crate::{
    QuackResult, qp_bail_internal, qp_internal,
    quackpack::core::{
        compile::{BuildContext, compiler_package::CompilerPackage},
        storage::freeze::FreezeDep,
    },
};

use super::Compiler;

pub mod creating_graph;
pub mod reducing_graph;

#[cfg(test)]
mod tests;

#[derive(Debug)]
pub struct AllPackages {
    packages: HashMap<FreezeDep, RwLock<CompilerPackage>>,
}

impl AllPackages {
    pub fn package(&self, name: FreezeDep) -> QuackResult<&RwLock<CompilerPackage>> {
        self.packages
            .get(&name)
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
        // NOTE: [`determine_compilation_order`] fails if it has encountered a cycle.
        self.determine_compilation_order().map(drop)
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
    pub fn package(&self, name: FreezeDep) -> QuackResult<&RwLock<CompilerPackage>> {
        self.all_packages.package(name)
    }

    pub fn compile<T: Compiler>(&self, compiler: &T, bctx: &BuildContext<'_>) -> QuackResult<()> {
        for package in self.graph.determine_compilation_order()? {
            self.compile_single_package(compiler, package, bctx)?;
        }
        Ok(())
    }

    fn compile_single_package<T: Compiler>(
        &self,
        compiler: &T,
        node: &DependencyNode,
        bctx: &BuildContext<'_>,
    ) -> QuackResult<()> {
        let lock = self.package(node.node)?;
        debug!("locking `{}` for writing", node.node);
        let mut package = lock.write().expect("panick'ed");
        let mut deps_locks = vec![];
        for dep in &node.dependencies {
            deps_locks.push((self.package(dep.node)?, dep.node));
        }
        let deps_guards = deps_locks
            .into_iter()
            .map(|(lock, node)| {
                debug!("locking `{}` for reading", node);
                lock.read().expect("panick'ed")
            })
            .collect::<Vec<_>>();
        let deps = deps_guards
            .iter()
            .map(|guard| {
                let pkg: &CompilerPackage = guard;
                pkg
            })
            .collect::<Vec<&CompilerPackage>>();
        for dep in &deps {
            if !dep.was_compiled() {
                qp_bail_internal!(
                    "`{}` `{}` was not compiled",
                    dep.package_type(),
                    dep.package().as_freeze_dep()
                )
            }
        }
        compiler.compile_package(&package, &deps, bctx.profile)?;
        package.mark_as_compiled();
        Ok(())
    }
}
