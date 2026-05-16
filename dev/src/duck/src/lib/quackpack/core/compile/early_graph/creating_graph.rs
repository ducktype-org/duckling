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
    /// This method checks that the graph is complete, and that it is, in fact, a DAG.
    #[tracing::instrument(skip_all)]
    pub fn new(freeze: &VenvFreeze) -> QuackResult<Self> {
        let root = freeze.root();
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
        }
        let root = root.as_freeze_dep();
        Self::check_is_dag(root, &graph)?;
        Ok(Self { root, graph })
    }

    /// Helpers for [`new`](Self::new).
    fn check_is_dag(root: FreezeDep, dag: &HashMap<FreezeDep, DependencyNode>) -> QuackResult<()> {
        Self::check_is_complete_graph(root, dag)?;
        Self::check_no_cycles(root, dag)?;
        Ok(())
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

    /// Checks, that the `graph` rooted at `root` doesn't have cycles.
    #[tracing::instrument(skip_all)]
    fn check_no_cycles(
        root: FreezeDep,
        dag: &HashMap<FreezeDep, DependencyNode>,
    ) -> QuackResult<()> {
        debug!(%root, graph = ?dag, "checking cycles");
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
            order.push(current);
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
            let previous = order.pop();
            debug_assert_eq!(previous, Some(current));
            Ok(())
        }
        visit_impl(root, dag, &mut states, &mut order)
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
