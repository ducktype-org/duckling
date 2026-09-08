//! Entrypoints for creating a new [`EarlyGraph`] and friends.

use itertools::Itertools;
use tracing::debug;

use super::*;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::compiler_package::PackageType;
use crate::quackpack::core::solver::solver_freeze::SolverFreeze;
use crate::quackpack::core::{AnyPackage, PackageId};
use crate::{qp_bail, qp_bail_internal};

impl DependencyGraph {
    /// Create new [`DependencyGraph`] from the given freeze.
    ///
    /// This method checks that the graph is complete (all edge targets have their neighbours lists),
    /// as well as checking that only local dependencies possibly form cycles.
    #[tracing::instrument(skip_all)]
    fn new(freeze: &SolverFreeze) -> QuackResult<Self> {
        let mut graph = HashMap::new();
        for (pkg_id, pkg_freeze) in freeze.package_freezes.iter() {
            let pkg_deps = pkg_freeze
                .dependencies_realization
                .values()
                .map(|x| x.identity().into());
            graph.insert(pkg_id.identity().into(), DependencyNode::new(pkg_deps));
        }
        let root_identity = freeze.main_pkg.identity().into();
        Self::check_is_complete_graph(root_identity, &graph)?;
        Self::check_cycles_only_on_local_deps(root_identity, &graph)?;
        Ok(Self {
            root: root_identity,
            graph,
        })
    }

    /// Checks, whether `graph` rooted at `root` is complete.
    #[tracing::instrument(skip_all)]
    fn check_is_complete_graph(
        root: Identity,
        graph: &HashMap<Identity, DependencyNode>,
    ) -> QuackResult<()> {
        debug!(%root, ?graph, "checking completeness");
        for (k, v) in graph {
            for dep in v.dependencies() {
                if !graph.contains_key(dep) {
                    let dep_type = if *k == root {
                        PackageType::DirectDependency
                    } else {
                        PackageType::TransitiveDependency
                    };
                    qp_bail_internal!(
                        "malformed freezefile: missing {dep_type} `{dep}`; {graph:#?}"
                    )
                }
            }
        }
        Ok(())
    }

    /// Checks that only cycles of dependencies are of local dependencies.
    fn check_cycles_only_on_local_deps(
        root: Identity,
        graph: &HashMap<Identity, DependencyNode>,
    ) -> QuackResult<()> {
        debug!(%root, ?graph, "checking cycles only on local deps");
        let sccs = kosaraju_sccs(graph);
        for scc in sccs {
            if scc.len() == 1 {
                // 1-element scc, so no cycle.
                continue;
            }
            for dep in scc {
                let is_local = dep.origin().kind().is_local();
                if !is_local {
                    // @TODO: #2603 Maybe this could be a better error message (find and print the whole cycle).
                    // But it is some effort (we have to pick any other vertex and do dfs to it and from it).
                    // Maybe print the whole ssc?
                    qp_bail!(
                        "the freeze contains a cycle of dependencies which contains a non-local package {}",
                        dep
                    );
                }
            }
        }
        Ok(())
    }
}

impl DependencyNode {
    /// Creates a new [`DependencyNode`] with the given dependencies.
    pub(super) fn new(dependencies: impl IntoIterator<Item = Identity>) -> Self {
        Self {
            dependencies: dependencies
                .into_iter()
                .sorted_by(|l, r| Identity::stable_compare(*l, *r))
                .collect(),
        }
    }
}

impl EarlyGraph {
    /// Creates a new [`EarlyGraph`] from the given [`BuildContext`].
    ///
    /// Also note that:
    /// - no features are expanded (including the root package),
    /// - no disabled dependencies are removed.
    #[tracing::instrument(skip_all)]
    pub(super) fn new_early(
        bcx: &BuildContext<'_, '_>,
        pkgs: Vec<(PackageId, AnyPackage)>,
    ) -> QuackResult<Self> {
        // `new` checks for cycles.
        let graph = DependencyGraph::new(&bcx.freeze)?;
        let mut packages = HashMap::new();
        let direct_dependencies_names = bcx
            .pcx
            .package()
            .manifest()
            .dependencies()
            .all_dependencies()
            .iter()
            .map(|dep| dep.name())
            .collect::<HashSet<_>>();
        for (pkg_id, pkg) in pkgs {
            let pkg_type = if Into::<Identity>::into(pkg_id.identity()) == bcx.root_identity {
                PackageType::RootPackage
            } else if direct_dependencies_names.contains(&pkg.name()) {
                PackageType::DirectDependency
            } else {
                PackageType::TransitiveDependency
            };
            let compiler_package = CompilerPackage::new(pkg, pkg_type);
            packages.insert(pkg_id.identity().into(), compiler_package);
        }
        Ok(Self {
            graph,
            packages: PackagesSet { inner: packages },
        })
    }
}

/// Computes the decomposition of the graph into strongly connected components.
/// Returns a vector of vectors, each vector lists vertices in one stronly connected component.
fn kosaraju_sccs(graph: &HashMap<Identity, DependencyNode>) -> Vec<Vec<Identity>> {
    // This is the Kosaraju's algorithm for finding strongly connected components of a graph.
    // Step 1: First DFS to get finishing order
    let mut visited = HashSet::new();
    let mut order = Vec::new();

    for vertex in graph.keys() {
        if !visited.contains(vertex) {
            dfs1(graph, *vertex, &mut visited, &mut order);
        }
    }

    // Step 2: Build reversed graph.
    let reversed = reverse_graph(graph);

    // Step 3: DFS on reversed graph in post-order of the first dfs.
    let mut visited = HashSet::new();
    let mut sccs = Vec::new();

    for vertex in order.iter().rev() {
        if !visited.contains(vertex) {
            let mut scc = Vec::new();
            dfs2(&reversed, *vertex, &mut visited, &mut scc);
            sccs.push(scc);
        }
    }

    sccs
}

/// Helper for [`kosaraju_sccs`].
/// Forwards DFS pass.
fn dfs1(
    graph: &HashMap<Identity, DependencyNode>,
    vertex: Identity,
    visited: &mut HashSet<Identity>,
    order: &mut Vec<Identity>,
) {
    visited.insert(vertex);

    if let Some(neighbors) = graph.get(&vertex) {
        for neighbor in neighbors.dependencies() {
            if !visited.contains(neighbor) {
                dfs1(graph, *neighbor, visited, order);
            }
        }
    }

    order.push(vertex);
}

/// Helper for [`kosaraju_sccs`].
/// Backwards DFS pass.
fn dfs2(
    graph: &HashMap<Identity, DependencyNode>,
    vertex: Identity,
    visited: &mut HashSet<Identity>,
    scc: &mut Vec<Identity>,
) {
    visited.insert(vertex);
    scc.push(vertex);

    if let Some(neighbors) = graph.get(&vertex) {
        for neighbor in neighbors.dependencies() {
            if !visited.contains(neighbor) {
                dfs2(graph, *neighbor, visited, scc);
            }
        }
    }
}

/// Helper for [`kosaraju_sccs`].
/// Computes the reversal of the dependencies graph.
fn reverse_graph(graph: &HashMap<Identity, DependencyNode>) -> HashMap<Identity, DependencyNode> {
    let mut reversed = HashMap::new();

    // Initialize all vertices
    for vertex in graph.keys() {
        reversed.insert(*vertex, DependencyNode::new(vec![]));
    }

    // Add reversed edges
    for (from, to_list) in graph {
        for to in to_list.dependencies() {
            reversed
                .entry(*to)
                .or_insert_with(DependencyNode::default)
                .dependencies_mut()
                .push(*from);
        }
    }

    reversed
}

/// Create a fully-ready [`EarlyGraph`] from the [`BuildContext`].
pub fn create_early_graph_from_bcx(
    bcx: &BuildContext<'_, '_>,
    pkgs: Vec<(PackageId, AnyPackage)>,
) -> QuackResult<EarlyGraph> {
    let mut graph = EarlyGraph::new_early(bcx, pkgs)?;
    graph.populate_features(&bcx.used_features)?;
    graph.remove_disabled_dependencies();
    Ok(graph)
}
