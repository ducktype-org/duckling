use crate::{
    DuckCtx, QpCtx, QuackResultContext, qp_bail,
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

impl DependencyGraph {
    pub fn new(freeze: &VenvFreeze) -> QuackResult<Self> {
        let root = freeze.root();
        let mut root_dependencies = vec![];
        let mut visited = vec![root.as_freeze_dep()];
        for dep in root.dependencies() {
            root_dependencies.push(DependencyNode::new(
                *dep,
                freeze,
                &mut visited,
                PackageType::DirectDependency,
            )?);
        }
        Ok(Self {
            root: DependencyNode {
                node: root.as_freeze_dep(),
                dependencies: root_dependencies,
            },
        })
    }

    pub fn reverse_topo_sort_order(&self) -> QuackResult<Vec<&DependencyNode>> {
        let mut order = vec![];
        let mut visited = HashSet::new();
        visit_impl(&self.root, &mut order, &mut visited)?;
        Ok(order)
    }
}

fn visit_impl<'a>(
    current: &'a DependencyNode,
    order: &mut Vec<&'a DependencyNode>,
    visited: &mut HashSet<FreezeDep>,
) -> QuackResult<()> {
    let inserted_new_entry = visited.insert(current.node);
    if !inserted_new_entry {
        let cycle = order
            .iter()
            .map(|node| node.node)
            .chain([current.node()])
            .map(|dep| format!("`{}`", dep))
            .join(" -> ");
        qp_bail!("malformed freezefile: cycle {cycle}")
    }
    for dep in &current.dependencies {
        if !visited.contains(&dep.node) {
            visit_impl(dep, order, visited)?;
        }
    }
    order.push(current);
    Ok(())
}

impl DependencyNode {
    pub fn new(
        node: FreezeDep,
        freeze: &VenvFreeze,
        visited: &mut Vec<FreezeDep>,
        pkg_type: PackageType,
    ) -> QuackResult<Self> {
        let has_cycle = visited.contains(&node);
        // Push it nevertheless, for a better error message.
        visited.push(node);
        if has_cycle {
            let cycle = visited.iter().map(|dep| format!("`{}`", dep)).join(" -> ");
            qp_bail!("malformed freezefile: cycle {cycle}")
        }
        let Some(entry) = freeze
            .dependencies()
            .iter()
            .find(|dep| dep.as_freeze_dep() == node)
        else {
            qp_bail!("malformed freezefile: missing {pkg_type} `{node}`")
        };
        let mut dependencies = vec![];
        for dep in entry.dependencies() {
            dependencies.push(DependencyNode::new(
                *dep,
                freeze,
                visited,
                pkg_type.deepen(),
            )?);
        }
        let popped = visited.pop();
        debug_assert_eq!(popped, Some(node));
        Ok(Self { node, dependencies })
    }
}

fn parse_dependency(
    dep: &FreezePackage,
    storage: &Storage,
    ctx: &DuckCtx,
    pkg_type: PackageType,
) -> QuackResult<CompilerPackage> {
    let storage_id = dep.to_package_id();
    let directory = storage.pkg_dir(&storage_id);
    let ctx = QpCtx::new(ctx);
    let ctx =
        PackageLoader::find_at_exact_directory(&directory, &ctx).with_context(
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

impl CompilerGraph {
    pub fn new_early(ctx: &BuildContext<'_>) -> QuackResult<Self> {
        let graph = DependencyGraph::new(&ctx.freeze)?;
        graph.bail_if_has_cycles()?;
        let mut packages = HashMap::new();
        packages.insert(
            ctx.freeze.root().as_freeze_dep(),
            CompilerPackage::new(ctx.package.package().clone(), PackageType::RootPackage),
        );
        for dep in ctx.freeze.dependencies() {
            let pkg_type = if ctx
                .package
                .package()
                .manifest()
                .dependencies()
                .has_dependency(dep.name())
            {
                PackageType::DirectDependency
            } else {
                PackageType::TransientDependency
            };
            let package = parse_dependency(dep, &ctx.storage, ctx.duck_ctx, pkg_type)?;
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
            all_packages: AllPackages { packages },
        })
    }
}
