//! Entrypoints for creating a new [`EarlyGraph`] and friends.

use tracing::debug;

use super::*;
use crate::quackpack::core::compile::BuildContext;
use crate::quackpack::core::compile::compiler_package::PackageType;
use crate::quackpack::core::storage::freeze::{FreezePackage, VenvFreeze};
use crate::quackpack::core::storage::package_id::PackageId;
use crate::quackpack::core::storage::paths::Storage;
use crate::quackpack::core::{Manifest, PackageLoader};
use crate::quackpack::util::to_path_buf::ToPathBuf;
use crate::{DuckContext, QuackResultContext, qp_bail, qp_bail_internal, qp_err};

impl DependencyGraph {
    /// Create new [`DependencyGraph`] from the given freeze.
    ///
    /// This method checks that the graph is complete (all edge targets have their neighbours lists),
    /// as well as checking that only local dependencies possibly form cycles.
    #[tracing::instrument(skip_all)]
    pub fn new(freeze: &VenvFreeze, root_identity: Identity) -> QuackResult<Self> {
        let root = freeze.root();
        let mut graph = HashMap::new();
        graph.insert(
            root_identity,
            DependencyNode::new(root.dependencies().iter().copied()),
        );
        for dep in freeze.dependencies() {
            graph.insert(
                dep.as_identity(),
                DependencyNode::new(dep.dependencies().iter().copied()),
            );
        }
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
                    qp_bail_internal!("malformed freezefile: missing {dep_type} `{dep}`")
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
    pub fn new(dependencies: impl IntoIterator<Item = Identity>) -> Self {
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
        PackageId::Local(ref local) => local.path().to_path_buf()?,
        _ => storage.pkg_dir(&storage_id),
    };
    let ctx =
        PackageLoader::find_at_exact_directory(&directory, ctx).with_context(
            || match storage_id {
                PackageId::Registry(ref registry_id) => format!(
                    "downloaded malformed dependency `{}` from `{}`",
                    dep.as_identity(),
                    registry_id.url()
                ),
                PackageId::Git(ref git_id) => format!(
                    "cloned malformed dependency `{}` from `{}`",
                    dep.as_identity(),
                    git_id.url()
                ),
                PackageId::Local(..) => format!(
                    "malformed local dependency `{}` at `{}`",
                    dep.as_identity(),
                    directory.display(),
                ),
            },
        )?;
    let package = ctx.package_arc();
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
        let graph = DependencyGraph::new(&bcx.freeze, bcx.root_identity)?;
        let mut packages = HashMap::new();
        packages.insert(
            bcx.root_identity,
            CompilerPackage::new(bcx.pcx.package_arc(), PackageType::RootPackage),
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
            let manifest = package.package().manifest();
            if manifest.name() != dep.name() || manifest.version() != dep.version() {
                return Err(error_for_metadata_mismtach(manifest, dep));
            }
            let overwritten_entry = packages.insert(dep.as_identity(), package).is_some();
            if overwritten_entry {
                qp_bail!(
                    "malformed freezefile: duplicated dependency `{}`",
                    dep.as_identity()
                )
            }
        }
        Ok(Self {
            graph,
            packages: PackagesSet { inner: packages },
        })
    }
}

/// Get the error message emitted when parsed package has different version (or name), than in the freeze.
fn error_for_metadata_mismtach(manifest: &Manifest, dep: &FreezePackage) -> QuackError {
    let display_expected = format!("{} {}", dep.name(), dep.version());
    let display_found = format!("{} {}", manifest.name(), manifest.version());
    match dep.to_package_id() {
        PackageId::Registry(registry_id) => qp_err!(
            "downloaded malformed dependency from `{}`: got name `{}`, expected `{}`",
            registry_id.url(),
            display_found,
            display_expected
        ),
        PackageId::Git(git_id) => qp_err!(
            "cloned malformed dependency from `{}`: got name `{}`, expected `{}`",
            git_id.url(),
            display_found,
            display_expected
        ),
        PackageId::Local(id) => qp_err!(
            "malformed local dependency at `{}`: got name `{}`, expected `{}`",
            id.path(),
            display_found,
            display_expected
        ),
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
