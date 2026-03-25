//! Dependency DAG and dictionary with all parsed [`CompilerPackage`]s.

use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::{
    QuackError, QuackResult, QuackResultContext,
    quackpack::core::{compile::compiler_package::CompilerPackage, storage::freeze::FreezeDep},
    util_common::error::MessageError,
};

pub mod creating_dag;
pub mod modifying_dag;

/// Common helper for creating a consistent error.
fn bail_cycle_message(cycle: &[FreezeDep]) -> QuackError {
    let cycle = cycle.iter().map(|dep| format!("`{}`", dep)).join(" -> ");
    MessageError(format!("malformed freezefile: cycle {cycle}").into()).into()
}

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

    /// Sort topologically this DAG.
    ///
    /// This method returns an error, if it encounters a cycle.
    pub fn topo_sort_order(&self) -> QuackResult<Vec<FreezeDep>> {
        #[derive(Debug, Eq, PartialEq)]
        enum State {
            Entered,
            Left,
        }

        let mut states = HashMap::new();
        let mut order = vec![];
        fn visit_impl(
            current: FreezeDep,
            dag: &HashMap<FreezeDep, DependencyNode>,
            states: &mut HashMap<FreezeDep, State>,
            order: &mut Vec<FreezeDep>,
        ) -> QuackResult<()> {
            let previous_state = states.insert(current, State::Entered);
            debug_assert_ne!(
                previous_state,
                Some(State::Left),
                "we shouldn't revisit nodes"
            );
            if previous_state == Some(State::Entered) {
                return Err(bail_cycle_message(order));
            }
            let deps = dag
                .get(&current)
                .expect("we've verified that there are dependencies");
            for dep in deps.dependencies() {
                if states.get(dep) != Some(&State::Left) {
                    visit_impl(*dep, dag, states, order)?;
                }
            }

            states.insert(current, State::Left);
            order.push(current);
            Ok(())
        }
        visit_impl(self.root, &self.dag, &mut states, &mut order)?;
        order.reverse();
        Ok(order)
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
