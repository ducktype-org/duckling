//! [`UnitGraph`] is a lowered version of [`EarlyGraph`].

use std::collections::{HashMap, VecDeque};

use tracing::{debug, instrument};

use super::{ArtifactsType, BuildKind, Unit};
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::early_graph::{DependencyNode, EarlyGraph};
use crate::quackpack::core::compile::{BuildContext, missing_depenendcy_in_graph};
use crate::quackpack::core::identity::Identity;

#[derive(Debug)]
/// A lowered version of the [`EarlyGraph`].
pub struct UnitGraph {
    root_id: u64,
    units: Vec<Unit>,
}

impl UnitGraph {
    /// Create a new [`UnitGraph`].
    pub fn new(root_id: u64, units: Vec<Unit>) -> Self {
        let ids = units.iter().map(Unit::unit_id);
        debug_assert!(ids.is_sorted(), "Units should be sorted by IDs; {units:?}");
        // Leaving `root_id` as a variable/member, since it might change in the future.
        debug_assert_eq!(root_id, 0, "root Unit should have an ID == 0");
        for (index, unit) in units.iter().enumerate() {
            debug_assert_eq!(
                index as u64,
                unit.unit_id(),
                "{index}-th Unit({}) should have ID == {index}, but has {}",
                unit.identity(),
                unit.unit_id(),
            );
        }
        Self { root_id, units }
    }

    /// Get the dependency for the given id.
    pub fn unit_for(&self, id: u64) -> &Unit {
        &self.units[id as usize]
    }

    /// Get the root [`Unit`].
    pub fn root_unit(&self) -> &Unit {
        self.unit_for(self.root_id)
    }

    /// Get [`Unit`]s sorted by their IDs.
    pub fn units_sorted_by_id(&self) -> &[Unit] {
        &self.units
    }

    /// Get the compilation order.
    pub fn compilation_order(&self) -> impl Iterator<Item = &'_ Unit> {
        self.units_sorted_by_id().iter().rev()
    }

    /// Check if the given [`Unit`] is the root [`Unit`].
    pub fn is_root(&self, unit: &Unit) -> bool {
        unit == self.root_unit()
    }
}

/// Lower an [`EarlyGraph`] to the [`UnitGraph`].
#[instrument(skip_all)]
pub fn lower_early_graph(graph: EarlyGraph, bcx: &BuildContext<'_, '_>) -> UnitGraph {
    let (identity_to_id, sorted_identities) = build_ids_map(&graph);
    debug!(?identity_to_id, ?sorted_identities);

    let (packages, graph) = graph.into_inner();
    let mut packages = packages.into_inner();
    let root_identity = graph.root();
    let root_id = *identity_to_id
        .get(&root_identity)
        .unwrap_or_else(|| missing_depenendcy_in_graph(root_identity, &identity_to_id));

    let mut units = vec![];
    for identity in sorted_identities {
        let package = packages
            .remove(&identity)
            .unwrap_or_else(|| missing_depenendcy_in_graph(identity, &identity_to_id));
        let node = graph.dependencies_for_package(&identity);
        let unit = create_single_unit(identity, root_identity, &identity_to_id, package, node, bcx);
        units.push(unit)
    }
    UnitGraph::new(root_id, units)
}

/// Create a map of `Identity -> UnitId`.
/// We have to do it __before__ creating any [`Unit`], since:
/// * [`Unit::new`] takes ID's of all its dependencies,
/// * we allow cycles, therefore we must obtain all ID's before.
///
/// IDs are assigned in the following order:
/// * root gets ID 0,
/// * next IDs are assigned in the “sorted” BFS orders; i.e.: dependency with the smallest
///   lexicographical name gets ID 1, next gets ID 2, and so on; then we assign IDs to the
///   dependencies of “1”, then “2”, and so on.
fn build_ids_map(graph: &EarlyGraph) -> (HashMap<Identity, u64>, Vec<Identity>) {
    let root = graph.graph().root();
    let mut next_available_id = 0;
    let mut result = HashMap::new();
    let mut sorted_identities = vec![];
    let mut queue = VecDeque::from([root]);
    while let Some(current) = queue.pop_front() {
        if result.contains_key(&current) {
            continue;
        }
        result.insert(current, next_available_id);
        next_available_id += 1;
        sorted_identities.push(current);
        let deps = graph
            .graph()
            .dependencies_for_package(&current)
            .dependencies()
            .to_vec();
        let deps = stable_sort_identities(deps);
        queue.extend(deps);
    }
    (result, sorted_identities)
}

/// Stable sort identities.
fn stable_sort_identities(mut identities: Vec<Identity>) -> Vec<Identity> {
    identities.sort_by(|lhs, rhs| Identity::stable_compare(*lhs, *rhs));
    identities
}

/// Lower [`EarlyGraph`] information into a single [`Unit`].
fn create_single_unit(
    unit_identity: Identity,
    root_identity: Identity,
    ids_map: &HashMap<Identity, u64>,
    package: CompilerPackage,
    node: &DependencyNode,
    bcx: &BuildContext<'_, '_>,
) -> Unit {
    let artifacts_type = infer_artifacts_type(unit_identity, root_identity, bcx);
    let mut dependencies = node
        .dependencies()
        .iter()
        .map(|dep| {
            *ids_map
                .get(dep)
                .unwrap_or_else(|| missing_depenendcy_in_graph(*dep, ids_map))
        })
        .collect::<Vec<_>>();
    dependencies.sort();
    let unit_id = *ids_map
        .get(&unit_identity)
        .unwrap_or_else(|| missing_depenendcy_in_graph(unit_identity, ids_map));
    Unit::new(
        unit_id,
        package,
        unit_identity,
        dependencies,
        artifacts_type,
        BuildKind::Compile,
    )
}

/// Infer an appropriate [`ArtifactsType`].
fn infer_artifacts_type(
    unit_identity: Identity,
    root_identity: Identity,
    bcx: &BuildContext<'_, '_>,
) -> ArtifactsType {
    let is_dvm = bcx.profile.dvm_bytecode;
    let is_root = unit_identity == root_identity;
    if is_dvm && is_root {
        return ArtifactsType::Dvm;
    }
    if is_root {
        return ArtifactsType::Binary;
    }
    // NOTE: Dependencies don't get their own tasks when compiling into DVM.
    ArtifactsType::IsADependencyArtifact
}
