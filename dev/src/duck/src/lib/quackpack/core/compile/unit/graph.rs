//! [`UnitGraph`] is a lowered version of [`EarlyGraph`].

use std::cell::RefCell;
use std::collections::{HashMap, VecDeque};

use tracing::instrument;

use super::{ArtifactsType, BuildKind, Unit, UnitId};
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::early_graph::{DependencyGraph, DependencyNode, EarlyGraph};
use crate::quackpack::core::compile::{BuildContext, missing_depenendcy_in_graph};
use crate::quackpack::core::identity::Identity;

#[derive(Debug)]
/// A lowered version of the [`EarlyGraph`].
pub struct UnitGraph {
    root_id: UnitId,
    units: Vec<(Unit, Vec<UnitId>)>,
}

pub type UnitIdsIter<'a> =
    core::iter::Map<core::slice::Iter<'a, (Unit, Vec<UnitId>)>, fn(&(Unit, Vec<UnitId>)) -> &Unit>;

impl UnitGraph {
    /// Create a new [`UnitGraph`].
    pub fn new(root_id: UnitId, units: Vec<(Unit, Vec<UnitId>)>) -> Self {
        if cfg!(debug_assertions) {
            assert_valid_units_order(&units);
        }
        debug_assert_eq!(root_id, 0, "root Unit should have an ID == 0");
        Self { root_id, units }
    }

    /// Get the dependency for the given id.
    pub fn unit_for(&self, id: UnitId) -> &Unit {
        &self.units[id as usize].0
    }

    /// Get the dependency for the given id.
    pub fn deps_for(&self, id: UnitId) -> &[UnitId] {
        &self.units[id as usize].1
    }

    /// Get the root [`Unit`].
    pub fn root_unit(&self) -> &Unit {
        self.unit_for(self.root_id)
    }

    /// Get [`Unit`]s sorted by their IDs.
    // NOTE: We need the entire type so implemented traits propagate.
    pub fn units_sorted_by_id(&self) -> UnitIdsIter<'_> {
        self.units.iter().map(|(unit, _)| unit)
    }

    /// Get the compilation order.
    pub fn compilation_order(&self) -> impl Iterator<Item = &'_ Unit> {
        self.units_sorted_by_id().rev()
    }

    /// Check if the given [`Unit`] is the root [`Unit`].
    pub fn is_root(&self, unit: &Unit) -> bool {
        unit == self.root_unit()
    }
}

fn assert_valid_units_order(units: &[(Unit, Vec<UnitId>)]) {
    for (index, (unit, unit_deps)) in units.iter().enumerate() {
        assert_eq!(
            index as UnitId,
            unit.unit_id(),
            "{index}-th Unit({}) should have ID == {index}, but has {}",
            unit.identity(),
            unit.unit_id(),
        );
        assert!(
            unit_deps.is_sorted(),
            "unit deps should be sorted {unit:?}, {unit_deps:?}"
        );
    }
}

/// Lower an [`EarlyGraph`] to the [`UnitGraph`].
#[instrument(skip_all)]
pub fn lower_early_graph(graph: EarlyGraph, bcx: &BuildContext<'_, '_>) -> UnitGraph {
    let builder = UnitGraphBuilder::new(graph);
    builder.lower(bcx)
}

#[derive(Debug)]
struct UnitGraphBuilder {
    topo_sorted_identities: HashMap<Identity, u64>,
    sorted_identities: Vec<Identity>,
    packages: RefCell<HashMap<Identity, CompilerPackage>>,
    graph: DependencyGraph,
    created_units_with_deps: RefCell<HashMap<Unit, Vec<UnitId>>>,
}

impl UnitGraphBuilder {
    fn new(graph: EarlyGraph) -> Self {
        let (topo_sorted_identities, sorted_identities) = build_ids_map(&graph);
        let (packages, graph) = graph.into_inner();
        let packages = packages.into_inner();
        Self {
            topo_sorted_identities,
            sorted_identities,
            packages: RefCell::new(packages),
            graph,
            created_units_with_deps: RefCell::default(),
        }
    }

    fn next_available_id(&self) -> UnitId {
        self.created_units_with_deps.borrow().len() as UnitId
    }

    fn lower(self, bcx: &BuildContext<'_, '_>) -> UnitGraph {
        self.populate_units(bcx);
        self.finish_lowering()
    }

    fn finish_lowering(self) -> UnitGraph {
        let units = self.created_units_with_deps.take();
        let mut units = units.into_iter().collect::<Vec<_>>();
        units.iter_mut().for_each(|(_, deps)| {
            deps.sort();
            deps.dedup();
        });
        let root_id = units
            .iter()
            .find_map(|(unit, _)| (unit.identity() == self.root_identity()).then(|| unit.unit_id()))
            .expect("missing root");
        units.sort_by_key(|(unit, _)| unit.unit_id());
        UnitGraph::new(root_id, units)
    }

    fn root_identity(&self) -> Identity {
        self.graph.root()
    }

    fn populate_units(&self, bcx: &BuildContext<'_, '_>) {
        for identity in self.sorted_identities.iter().copied() {
            let package = self
                .packages
                .borrow_mut()
                .remove(&identity)
                .unwrap_or_else(|| {
                    missing_depenendcy_in_graph(identity, &self.topo_sorted_identities)
                });
            let node = self.graph.dependencies_for_package(&identity);
            let unit = self.create_single_unit(identity, package, bcx);
            self.populate_unit_deps(&unit, node);
        }
    }

    fn create_single_unit(
        &self,
        unit_identity: Identity,
        package: CompilerPackage,
        bcx: &BuildContext<'_, '_>,
    ) -> Unit {
        let artifacts_type = infer_artifacts_type(unit_identity, self.root_identity(), bcx);
        let unit_id = self.next_available_id();
        let unit = Unit::new(
            unit_id,
            package,
            unit_identity,
            // dependencies,
            artifacts_type,
            BuildKind::Compile,
        );
        self.created_units_with_deps
            .borrow_mut()
            .entry(unit.clone())
            .or_default();
        unit
    }

    fn populate_unit_deps(&self, unit: &Unit, node: &DependencyNode) {
        let mut map = self.created_units_with_deps.borrow_mut();
        let deps = map.get_mut(unit).unwrap();
        node.dependencies().iter().for_each(|dep| {
            deps.push(*self.topo_sorted_identities.get(dep).unwrap_or_else(|| {
                missing_depenendcy_in_graph(*dep, &self.topo_sorted_identities)
            }));
        });
    }
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
