//! Entrypoints for creating a new [`EarlyDag`] and friends.

use crate::{
    DuckCtx, QuackResultContext, qp_bail, qp_bail_internal,
    quackpack::core::{
        PackageLoader,
        compile::{BuildContext, compiler_package::PackageType},
        storage::{
            freeze::{FreezePackage, VenvFreeze},
            package_id::PackageId,
            paths::Storage,
        },
    },
};

use super::*;

impl DependencyDag {
    /// Create new [`DependencyDag`] from the given freeze.
    ///
    /// This method checks that the graph is complete, and that it is, in fact, a DAG.
    #[tracing::instrument(skip_all)]
    pub fn new(freeze: &VenvFreeze) -> QuackResult<Self> {
        let root = freeze.root();
        let mut dag = HashMap::new();
        dag.insert(
            root.as_freeze_dep(),
            DependencyNode::new(root.dependencies().iter().copied()),
        );
        for dep in freeze.dependencies() {
            dag.insert(
                dep.as_freeze_dep(),
                DependencyNode::new(dep.dependencies().iter().copied()),
            );
        }
        let root = root.as_freeze_dep();
        Self::check_is_dag(root, &dag)?;
        Ok(Self { root, dag })
    }

    /// Helpers for [`new`](Self::new).
    fn check_is_dag(root: FreezeDep, dag: &HashMap<FreezeDep, DependencyNode>) -> QuackResult<()> {
        Self::check_is_complete_graph(root, dag)?;
        Self::check_no_cycles(root, dag)?;
        Ok(())
    }

    /// Checks, whether `graph` rooted at `root` is complete.
    #[tracing::instrument]
    fn check_is_complete_graph(
        root: FreezeDep,
        graph: &HashMap<FreezeDep, DependencyNode>,
    ) -> QuackResult<()> {
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
    #[tracing::instrument]
    fn check_no_cycles(
        root: FreezeDep,
        dag: &HashMap<FreezeDep, DependencyNode>,
    ) -> QuackResult<()> {
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
#[tracing::instrument(skip(storage, ctx))]
fn parse_dependency(
    dep: &FreezePackage,
    storage: &Storage,
    ctx: &DuckCtx,
    pkg_type: PackageType,
) -> QuackResult<CompilerPackage> {
    let storage_id = dep.to_package_id();
    let directory = storage.pkg_dir(&storage_id);
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
                PackageId::Local(ref local_id) => format!(
                    "malformed local dependency `{}` at `{}`",
                    dep.as_freeze_dep(),
                    local_id.path().display()
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
            PackageId::Local(local_id) => qp_bail!(
                "malformed local dependency at `{}`: got name `{}`, expected `{}`",
                local_id.path().display(),
                package.as_freeze_dep(),
                dep.as_freeze_dep(),
            ),
        }
    }
    Ok(CompilerPackage::new(package, pkg_type))
}

impl EarlyDag {
    /// Creates a new [`EarlyDag`] from the given [`BuildContext`].
    ///
    /// Also note that:
    /// - no features are expanded (including the root package),
    /// - no disabled dependencies are removed.
    #[tracing::instrument(skip(ctx))]
    pub fn new_early(ctx: &BuildContext<'_, '_>) -> QuackResult<Self> {
        // `new` checks for cycles.
        let graph = DependencyDag::new(&ctx.freeze)?;
        let mut packages = HashMap::new();
        packages.insert(
            ctx.freeze.root().as_freeze_dep(),
            CompilerPackage::new(ctx.package.package().clone(), PackageType::RootPackage),
        );
        let direct_dependencies_names = ctx
            .package
            .package()
            .manifest()
            .dependencies()
            .all_dependencies()
            .iter()
            .map(|dep| dep.name())
            .collect::<HashSet<_>>();
        for dep in ctx.freeze.dependencies() {
            let pkg_type = if direct_dependencies_names.contains(&dep.name()) {
                PackageType::DirectDependency
            } else {
                PackageType::TransitiveDependency
            };
            let package = parse_dependency(dep, &ctx.storage, ctx.package.ctx(), pkg_type)?;
            let overwritten_entry = packages.insert(dep.as_freeze_dep(), package).is_some();
            if overwritten_entry {
                qp_bail!(
                    "malformed freezefile: duplicated dependency `{}`",
                    dep.as_freeze_dep()
                )
            }
        }
        Ok(Self {
            dag: graph,
            packages: PackagesSet { inner: packages },
        })
    }
}
