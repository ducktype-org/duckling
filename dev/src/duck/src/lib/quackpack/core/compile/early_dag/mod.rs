//! Dependency DAG and dictionary with all parsed [`CompilerPackage`]s.

use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::quackpack::core::compile::MISSING_DEPENDENCY_IN_DAG_MESSAGE;
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::identity::Identity;
use crate::util::error::MessageError;
use crate::{QuackError, QuackResult};

pub mod creating_dag;
pub mod modifying_dag;

/// Common helper for creating a consistent error.
fn bail_cycle_message(cycle: &[Identity]) -> QuackError {
    let cycle = cycle.iter().map(|dep| format!("`{}`", dep)).join(" -> ");
    MessageError(format!("malformed freezefile: cycle {cycle}").into()).into()
}

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
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &Identity) -> &mut CompilerPackage {
        self.inner
            .get_mut(name)
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
    }
}

#[derive(Debug)]
/// Dependency DAG ([`DependencyDag`]) + packages cache ([`PackagesSet`]).
pub struct EarlyDag {
    packages: PackagesSet,
    dag: DependencyDag,
}

#[derive(Debug, PartialEq, Eq)]
/// DAG representing dependency-dependant relations.
pub struct DependencyDag {
    root: Identity,
    dag: HashMap<Identity, DependencyNode>,
}

#[derive(Debug, PartialEq, Eq, Hash, Default)]
/// Single dependency node.
///
/// Contains dependencies of this node.
pub struct DependencyNode {
    dependencies: Vec<Identity>,
}

impl DependencyDag {
    /// Get the root package of this DAG.
    pub fn root(&self) -> Identity {
        self.root
    }

    /// Get the [`DependencyNode`] for the given package.
    pub fn dependencies_for_package(&self, package: &Identity) -> &DependencyNode {
        self.dag
            .get(package)
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
    }

    /// Sort topologically this DAG.
    ///
    /// This method returns an error, if it encounters a cycle.
    pub fn topo_sort_order(&self) -> QuackResult<Vec<Identity>> {
        #[derive(Debug, Eq, PartialEq)]
        enum State {
            Entered,
            Left,
        }

        let mut states = HashMap::new();
        let mut order = vec![];
        fn visit_impl(
            current: Identity,
            dag: &HashMap<Identity, DependencyNode>,
            states: &mut HashMap<Identity, State>,
            order: &mut Vec<Identity>,
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
    pub fn dependencies(&self) -> &[Identity] {
        &self.dependencies
    }

    /// Same as [`dependencies`](Self::dependencies), but returns a mutable vector.
    pub fn dependencies_mut(&mut self) -> &mut Vec<Identity> {
        &mut self.dependencies
    }
}

impl EarlyDag {
    /// Get the [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &Identity) -> &CompilerPackage {
        self.packages.package(name)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &Identity) -> &mut CompilerPackage {
        self.packages.package_mut(name)
    }

    /// Get the underlying [`DependencyDag`].
    pub fn dag(&self) -> &DependencyDag {
        &self.dag
    }
}
