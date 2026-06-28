//! [`UnitGraph`] is a lowered version of [`EarlyGraph`].

use std::collections::HashMap;

use super::{ArtifactsType, Unit};
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::early_graph::{DependencyNode, EarlyGraph};
use crate::quackpack::core::compile::{BuildContext, missing_depenendcy_in_graph_message};
use crate::quackpack::core::identity::Identity;

#[derive(Debug)]
/// A lowered version of the [`EarlyGraph`].
pub struct UnitGraph {
    root_id: u64,
    // `pub(super)` so that tests under `unit/tests` can access this field.
    // other solution is to move this definition to `unit/mod.rs`.
    pub(super) units: HashMap<u64, Unit>,
}

impl UnitGraph {
    /// Create a new [`UnitGraph`].
    pub fn new(root_id: u64, units: HashMap<u64, Unit>) -> Self {
        Self { root_id, units }
    }

    /// Get the dependency for the given id.
    pub fn unit_for(&self, id: u64) -> &Unit {
        self.units
            .get(&id)
            .unwrap_or_else(|| panic!("attempted to obtain a unit for nonexistent id=`{id}`"))
    }

    /// Get the root [`Unit`].
    pub fn root_unit(&self) -> &Unit {
        self.unit_for(self.root_id)
    }

    /// Get an iterator over [`Unit`]s, in any order.
    pub fn any_units_order(&self) -> impl Iterator<Item = &Unit> {
        self.units.values()
    }

    /// Check if the given [`Unit`] is the root [`Unit`].
    pub fn is_root(&self, unit: &Unit) -> bool {
        unit == self.root_unit()
    }
}

/// Lower an [`EarlyGraph`] to the [`UnitGraph`].
pub fn lower_early_graph(graph: EarlyGraph, bcx: &BuildContext<'_, '_>) -> UnitGraph {
    let identity_to_id = build_ids_map(&graph);

    let (packages, graph) = graph.into_inner();
    let packages = packages.into_inner();
    let root_identity = graph.root();
    let root_id = *identity_to_id
        .get(&root_identity)
        .unwrap_or_else(|| panic!("{}", missing_depenendcy_in_graph_message(root_identity)));

    let mut units_by_ids = HashMap::new();
    for (identity, package) in packages {
        let node = graph.dependencies_for_package(&identity);
        let unit = create_single_unit(identity, root_identity, &identity_to_id, package, node, bcx);
        let id = unit.unit_id();
        units_by_ids.insert(id, unit);
    }
    UnitGraph::new(root_id, units_by_ids)
}

/// Create a map of `Identity -> UnitId`.
/// We have to do it __before__ creating any [`Unit`], since:
/// * [`Unit::new`] takes ID's of all its dependencies,
/// * we allow cycles, therefore we must obtain all ID's before.
fn build_ids_map(graph: &EarlyGraph) -> HashMap<Identity, u64> {
    // !TODO: This gives us fairly unstable (even between duck invocations) IDs' order.
    // Do we care?
    graph
        .graph()
        .keys()
        .enumerate()
        .map(|(unit_id, identity)| (*identity, unit_id as u64))
        .collect()
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
    let dependencies = node
        .dependencies()
        .iter()
        .map(|dep| {
            *ids_map
                .get(dep)
                .unwrap_or_else(|| panic!("{}", missing_depenendcy_in_graph_message(*dep)))
        })
        .collect();
    let unit_id = *ids_map
        .get(&unit_identity)
        .unwrap_or_else(|| panic!("{}", missing_depenendcy_in_graph_message(unit_identity)));
    Unit::new(
        unit_id,
        package,
        unit_identity,
        dependencies,
        artifacts_type,
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
