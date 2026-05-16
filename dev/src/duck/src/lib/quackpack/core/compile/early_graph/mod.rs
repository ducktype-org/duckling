//! Dependency DAG and dictionary with all parsed [`CompilerPackage`]s.

use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::quackpack::core::compile::MISSING_DEPENDENCY_IN_DAG_MESSAGE;
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::storage::freeze::FreezeDep;
use crate::util::error::MessageError;
use crate::{QuackError, QuackResult};

pub mod creating_graph;
pub mod modifying_graph;

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
pub struct PackagesSet {
    inner: HashMap<FreezeDep, CompilerPackage>,
}

impl PackagesSet {
    /// Get [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &FreezeDep) -> &CompilerPackage {
        self.inner
            .get(name)
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &FreezeDep) -> &mut CompilerPackage {
        self.inner
            .get_mut(name)
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
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
    root: FreezeDep,
    graph: HashMap<FreezeDep, DependencyNode>,
}

#[derive(Debug, PartialEq, Eq, Hash, Default)]
/// Single dependency node.
///
/// Contains dependencies of this node.
pub struct DependencyNode {
    dependencies: Vec<FreezeDep>,
}

impl DependencyGraph {
    /// Get the root package of this graph.
    pub fn root(&self) -> FreezeDep {
        self.root
    }

    /// Get the [`DependencyNode`] for the given package.
    pub fn dependencies_for_package(&self, package: &FreezeDep) -> &DependencyNode {
        self.graph
            .get(package)
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
    }

    /// Sort topologically this graph.
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
        visit_impl(self.root, &self.graph, &mut states, &mut order)?;
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

impl EarlyGraph {
    /// Get the [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &FreezeDep) -> &CompilerPackage {
        self.packages.package(name)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &FreezeDep) -> &mut CompilerPackage {
        self.packages.package_mut(name)
    }

    /// Get the underlying [`DependencyGraph`].
    pub fn graph(&self) -> &DependencyGraph {
        &self.graph
    }
}
