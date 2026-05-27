//! Entrypoints for creating a new [`EarlyGraph`] and friends.

use tracing::debug;

use super::*;
use crate::quackpack::core::PackageLoader;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::compiler_package::PackageType;
use crate::quackpack::core::storage::freeze::{FreezePackage, VenvFreeze};
use crate::quackpack::core::storage::package_id::PackageId;
use crate::quackpack::core::storage::paths::Storage;
use crate::{DuckContext, QuackResultContext, qp_bail, qp_bail_internal};

impl DependencyGraph {
    /// Create new [`DependencyGraph`] from the given freeze.
    ///
    /// This method checks that the graph is complete (all edge targets have their neighbours lists).
    #[tracing::instrument(skip_all)]
    pub fn new(freeze: &VenvFreeze) -> QuackResult<Self> {
        let root = freeze.root();
        // @TODO: #2789 this is ugly, but with Identities, I think FreezeDep will contain Identity.
        let mut freeze_dep_to_freeze_pkg = HashMap::new();
        let mut graph = HashMap::new();
        graph.insert(
            root.as_freeze_dep(),
            DependencyNode::new(root.dependencies().iter().copied()),
        );
        for dep in freeze.dependencies() {
            graph.insert(
                dep.as_freeze_dep(),
                DependencyNode::new(dep.dependencies().iter().copied()),
            );
            freeze_dep_to_freeze_pkg.insert(dep.as_freeze_dep(), dep.clone());
        }
        let root = root.as_freeze_dep();
        Self::check_is_complete_graph(root, &graph)?;
        Self::check_cycles_only_on_local_deps(&freeze_dep_to_freeze_pkg, root, &graph)?;
        Ok(Self { root, graph })
    }

    /// Checks, whether `graph` rooted at `root` is complete.
    #[tracing::instrument(skip_all)]
    fn check_is_complete_graph(
        root: FreezeDep,
        graph: &HashMap<FreezeDep, DependencyNode>,
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
                    qp_bail_internal!("malformed freezefile: missing {dep_type} `{dep}`")
                }
            }
        }
        Ok(())
    }

    /// Checks that only cycles of dependencies are of local dependencies.
    fn check_cycles_only_on_local_deps(
        freeze_dep_to_freeze_pkg: &HashMap<FreezeDep, FreezePackage>,
        root: FreezeDep,
        graph: &HashMap<FreezeDep, DependencyNode>,
    ) -> QuackResult<()> {
        debug!(%root, ?graph, "checking cycles only on local deps");
        let sccs = kosaraju_sccs(graph);
        for scc in sccs {
            if scc.len() == 1 {
                // 1-element scc, so no cycle.
                continue;
            }
            for dep in scc {
                let mut is_local = true;
                if let Some(freeze_pkg) = freeze_dep_to_freeze_pkg.get(&dep) {
                    is_local = freeze_pkg.source().is_local();
                }
                // Otherwise this is the root package, so it is local.
                if !is_local {
                    // @TODO: #2789 Maybe this could be a better error message (find and print the whole cycle).
                    // But it is too much pain with this API right now.
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
    pub fn new(dependencies: impl IntoIterator<Item = FreezeDep>) -> Self {
        Self {
            dependencies: dependencies.into_iter().collect(),
        }
    }
}

/// Parses the dependency from the freezefile using data in the storage.
#[tracing::instrument(skip_all)]
fn parse_dependency(
    dep: &FreezePackage,
    storage: &Storage,
    ctx: &DuckContext,
    pkg_type: PackageType,
) -> QuackResult<CompilerPackage> {
    debug!(?dep, type = %pkg_type, "parsing dep");
    let storage_id = dep.to_package_id();
    let directory = match storage_id {
        PackageId::Local(ref local) => local.path().to_path_buf(),
        _ => storage.pkg_dir(&storage_id),
    };
    let ctx =
        PackageLoader::find_at_exact_directory(&directory, ctx).with_context(
            || match storage_id {
                PackageId::Registry(ref registry_id) => format!(
                    "downloaded malformed dependency `{}` from `{}`",
                    dep.as_freeze_dep(),
                    registry_id.url()
                ),
                PackageId::Git(ref git_id) => format!(
                    "cloned malformed dependency `{}` from `{}`",
                    dep.as_freeze_dep(),
                    git_id.url()
                ),
                PackageId::Local(..) => format!(
                    "malformed local dependency `{}` at `{}`",
                    dep.as_freeze_dep(),
                    directory.display(),
                ),
            },
        )?;
    let package = ctx.into_package();
    if package.as_freeze_dep() != dep.as_freeze_dep() {
        match storage_id {
            PackageId::Registry(registry_id) => qp_bail!(
                "downloaded malformed dependency from `{}`: got name `{}`, expected `{}`",
                registry_id.url(),
                package.as_freeze_dep(),
                dep.as_freeze_dep(),
            ),
            PackageId::Git(git_id) => qp_bail!(
                "cloned malformed dependency from `{}`: got name `{}`, expected `{}`",
                git_id.url(),
                package.as_freeze_dep(),
                dep.as_freeze_dep(),
            ),
            PackageId::Local(..) => qp_bail!(
                "malformed local dependency at `{}`: got name `{}`, expected `{}`",
                directory.display(),
                package.as_freeze_dep(),
                dep.as_freeze_dep(),
            ),
        }
    }
    Ok(CompilerPackage::new(package, pkg_type))
}

impl EarlyGraph {
    /// Creates a new [`EarlyGraph`] from the given [`BuildContext`].
    ///
    /// Also note that:
    /// - no features are expanded (including the root package),
    /// - no disabled dependencies are removed.
    #[tracing::instrument(skip_all)]
    pub fn new_early(bcx: &BuildContext<'_, '_>) -> QuackResult<Self> {
        // `new` checks for cycles.
        let graph = DependencyGraph::new(&bcx.freeze)?;
        let mut packages = HashMap::new();
        packages.insert(
            bcx.freeze.root().as_freeze_dep(),
            CompilerPackage::new(bcx.pcx.package().clone(), PackageType::RootPackage),
        );
        let direct_dependencies_names = bcx
            .pcx
            .package()
            .manifest()
            .dependencies()
            .all_dependencies()
            .iter()
            .map(|dep| dep.name())
            .collect::<HashSet<_>>();
        for dep in bcx.freeze.dependencies() {
            let pkg_type = if direct_dependencies_names.contains(&dep.name()) {
                PackageType::DirectDependency
            } else {
                PackageType::TransitiveDependency
            };
            let package = parse_dependency(dep, &bcx.storage, bcx.pcx.ctx(), pkg_type)?;
            let overwritten_entry = packages.insert(dep.as_freeze_dep(), package).is_some();
            if overwritten_entry {
                qp_bail!(
                    "malformed freezefile: duplicated dependency `{}`",
                    dep.as_freeze_dep()
                )
            }
        }
        Ok(Self {
            graph,
            packages: PackagesSet { inner: packages },
        })
    }
}

/// Computes the decomposition of the graph into strongly connected components.
/// Returns a vector of vectors, each vector lists vertices in one stronly connected component.
fn kosaraju_sccs(graph: &HashMap<FreezeDep, DependencyNode>) -> Vec<Vec<FreezeDep>> {
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
    graph: &HashMap<FreezeDep, DependencyNode>,
    vertex: FreezeDep,
    visited: &mut HashSet<FreezeDep>,
    order: &mut Vec<FreezeDep>,
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
    graph: &HashMap<FreezeDep, DependencyNode>,
    vertex: FreezeDep,
    visited: &mut HashSet<FreezeDep>,
    scc: &mut Vec<FreezeDep>,
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
fn reverse_graph(graph: &HashMap<FreezeDep, DependencyNode>) -> HashMap<FreezeDep, DependencyNode> {
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
