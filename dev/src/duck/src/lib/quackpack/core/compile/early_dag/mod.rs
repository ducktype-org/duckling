//! Dependency DAG and dictionary with all parsed [`CompilerPackage`]s.

use std::collections::{HashMap, HashSet};

use itertools::Itertools;

use crate::quackpack::core::compile::MISSING_DEPENDENCY_IN_DAG_MESSAGE;
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::simple_identity::SimpleIdentity;
use crate::util::error::MessageError;
use crate::{QuackError, QuackResult};

pub mod creating_dag;
pub mod modifying_dag;

/// Common helper for creating a consistent error.
fn bail_cycle_message(cycle: &[SimpleIdentity]) -> QuackError {
    let cycle = cycle.iter().map(|dep| format!("`{}`", dep)).join(" -> ");
    MessageError(format!("malformed freezefile: cycle {cycle}").into()).into()
}

#[cfg(test)]
mod tests;

#[derive(Debug)]
/// Dictionary [`SimpleIdentity`] -> [`CompilerPackage`].
///
/// *Should* contain root and all dependencies listed in a freezefile.
pub struct PackagesSet {
    inner: HashMap<SimpleIdentity, CompilerPackage>,
}

impl PackagesSet {
    /// Get [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &SimpleIdentity) -> &CompilerPackage {
        self.inner
            .get(name)
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &SimpleIdentity) -> &mut CompilerPackage {
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
    root: SimpleIdentity,
    dag: HashMap<SimpleIdentity, DependencyNode>,
}

#[derive(Debug, PartialEq, Eq, Hash, Default)]
/// Single dependency node.
///
/// Contains dependencies of this node.
pub struct DependencyNode {
    dependencies: Vec<SimpleIdentity>,
}

impl DependencyDag {
    /// Get the root package of this DAG.
    pub fn root(&self) -> SimpleIdentity {
        self.root
    }

    /// Get the [`DependencyNode`] for the given package.
    pub fn dependencies_for_package(&self, package: &SimpleIdentity) -> &DependencyNode {
        self.dag
            .get(package)
            .expect(MISSING_DEPENDENCY_IN_DAG_MESSAGE)
    }

    /// Sort topologically this DAG.
    ///
    /// This method returns an error, if it encounters a cycle.
    pub fn topo_sort_order(&self) -> QuackResult<Vec<SimpleIdentity>> {
        #[derive(Debug, Eq, PartialEq)]
        enum State {
            Entered,
            Left,
        }

        let mut states = HashMap::new();
        let mut order = vec![];
        fn visit_impl(
            current: SimpleIdentity,
            dag: &HashMap<SimpleIdentity, DependencyNode>,
            states: &mut HashMap<SimpleIdentity, State>,
            order: &mut Vec<SimpleIdentity>,
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
    pub fn dependencies(&self) -> &[SimpleIdentity] {
        &self.dependencies
    }

    /// Same as [`dependencies`](Self::dependencies), but returns a mutable vector.
    pub fn dependencies_mut(&mut self) -> &mut Vec<SimpleIdentity> {
        &mut self.dependencies
    }
}

impl EarlyDag {
    /// Get the [`CompilerPackage`] for the given `name`.
    pub fn package(&self, name: &SimpleIdentity) -> &CompilerPackage {
        self.packages.package(name)
    }

    /// Same as [`package`](Self::package), but returns a mutable reference.
    pub fn package_mut(&mut self, name: &SimpleIdentity) -> &mut CompilerPackage {
        self.packages.package_mut(name)
    }

    /// Get the underlying [`DependencyDag`].
    pub fn dag(&self) -> &DependencyDag {
        &self.dag
    }
}
