//! [`UnitGraph`] is a lowered version of [`EarlyGraph`].

use std::cell::RefCell;
use std::collections::{HashMap, VecDeque};

use tracing::instrument;

use super::{BuildKind, Unit, UnitId, UnitType};
use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::early_graph::{DependencyGraph, DependencyNode, EarlyGraph};
use crate::quackpack::core::compile::missing_depenendcy_in_graph;
use crate::quackpack::core::identity::Identity;

#[derive(Debug)]
/// A lowered version of the [`EarlyGraph`].
pub struct UnitGraph {
    root_id: UnitId,
    /// All known [`Unit`]s.
    ///
    /// Unit with [`UnitId`] == `i` is at i-th index in the vector.
    units: Vec<Unit>,
    /// All dependencies of [`Unit`]s.
    ///
    /// At index `i` we have dependencies of the [`Unit`] with [`UnitId`] == i.
    ///
    /// It has the same length as [`units`], which is guaranteed by [`new`] (we take vector of pairs
    /// and decompose it).
    ///
    /// [`units`]: Self::units
    /// [`new`]: Self::new
    dependencies: Vec<Vec<UnitId>>,
}

impl UnitGraph {
    /// Create a new [`UnitGraph`].
    pub fn new(root_id: UnitId, units: Vec<(Unit, Vec<UnitId>)>) -> Self {
        if cfg!(debug_assertions) {
            assert_valid_units_order(&units);
        }
        debug_assert_eq!(root_id, 0, "invalid root unit id");
        let (units, dependencies) = decompose_units(units);
        Self {
            root_id,
            units,
            dependencies,
        }
    }

    /// Get the dependency for the given id.
    pub fn unit_for(&self, id: UnitId) -> &Unit {
        &self.units[id as usize]
    }

    /// Get the dependency for the given id.
    pub fn deps_for(&self, id: UnitId) -> &[UnitId] {
        &self.dependencies[id as usize]
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
    pub fn compilation_order(&self) -> core::iter::Rev<core::slice::Iter<'_, Unit>> {
        self.units_sorted_by_id().iter().rev()
    }

    /// Check if the given [`Unit`] is the root [`Unit`].
    pub fn is_root(&self, unit: &Unit) -> bool {
        unit == self.root_unit()
    }
}

/// Check that the input to [`UnitGraph::new`] is valid.
///
/// This should be executed only under `cfg!(debug_assertions)`.
fn assert_valid_units_order(units: &[(Unit, Vec<UnitId>)]) {
    for (index, (unit, deps)) in units.iter().enumerate() {
        assert_eq!(
            index as UnitId,
            unit.unit_id(),
            "{index}-th Unit({}) should have ID == {index}, but has {}",
            unit.identity(),
            unit.unit_id(),
        );
        assert!(
            deps.is_sorted(),
            "unit `{unit:?}` deps are not sorted {deps:#?}"
        );
        let has_dups = deps.windows(2).any(|window| window[0] == window[1]);
        assert!(!has_dups, "unit `{unit:?}` deps have duplicates {deps:#?}");
    }
}

fn decompose_units(units: Vec<(Unit, Vec<UnitId>)>) -> (Vec<Unit>, Vec<Vec<UnitId>>) {
    units.into_iter().unzip()
}

/// Lower an [`EarlyGraph`] to the [`UnitGraph`].
#[instrument(skip_all)]
pub fn lower_early_graph(graph: EarlyGraph) -> UnitGraph {
    let builder = UnitGraphBuilder::new(graph);
    builder.lower()
}

#[derive(Debug)]
/// Helper for lowering an [`EarlyGraph`] to the [`UnitGraph`].
/// NOTE: `pub(crate)` indicates API to be used in [`lower_early_graph`], other functions are helpers.
pub(crate) struct UnitGraphBuilder {
    /// Map [`Identity`] -> unique id.
    /// It's guaranteed that ids are from range `(0..identities.len())`.
    /// For the order of ids and more details, see [`build_ids_map`].
    identity_to_id: HashMap<Identity, u64>,
    /// Map unique id -> [`Identity`].
    /// Because ids are from contiguous range, we store them in a vector.
    id_to_identity: Vec<Identity>,
    /// Map [`Identity`] -> [`CompilerPackage`].
    packages: RefCell<HashMap<Identity, CompilerPackage>>,
    graph: DependencyGraph,
    /// Created [`Unit`]s with their dependencies up to some point.
    /// The key-value pair is [`Unit`] -> ids of its dependencies.
    ///
    /// Some details about populating dependencies:
    /// * ids can be put in any order,
    /// * you can duplicate ids.
    ///
    /// In [`finish_lowering`] we sort everything + deduplicate.
    /// Note, that we __don't__ deduplicate [`Unit`]s.
    /// Each call to [`create_single_unit`] _always_ creates a new [`Unit`].
    ///
    /// [`finish_lowering`]: Self::finish_lowering
    /// [`create_single_unit`]: Self::create_single_unit
    created_units_with_deps: RefCell<HashMap<Unit, Vec<UnitId>>>,
}

impl UnitGraphBuilder {
    /// Create a new [`UnitGraphBuilder`] from the [`EarlyGraph`].
    pub(crate) fn new(graph: EarlyGraph) -> Self {
        let (identity_to_id, id_to_identity) = build_ids_map(&graph);
        let (packages, graph) = graph.into_inner();
        let packages = packages.into_inner();
        Self {
            identity_to_id,
            id_to_identity,
            packages: RefCell::new(packages),
            graph,
            created_units_with_deps: RefCell::default(),
        }
    }

    /// Lower (essentially) decomposed [`EarlyGraph`] (from [`new`]) to the [`UnitGraph`].
    ///
    /// [`new`]: Self::new
    pub(crate) fn lower(self) -> UnitGraph {
        self.populate_units();
        self.finish_lowering()
    }

    /// Get the next free for the next [`Unit`].
    fn next_available_id(&self) -> UnitId {
        self.created_units_with_deps.borrow().len() as UnitId
    }

    /// We have fully populated all fields (i.e. created all [`Unit`]s and added their
    /// dependencies).
    ///
    /// Transform this information into a [`UnitGraph`].
    ///
    /// This function will, additionally:
    /// * convert [`created_units_with_deps`] from a `HashMap` into a vector of pairs,
    /// * sort that vector by the key/first element of pair ([`Unit`]) by [`UnitId`],
    /// * sort and deduplicate every second element of tuple (dependencies of a [`Unit`]).
    ///
    /// [`created_units_with_deps`]: Self::created_units_with_deps
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

    /// Get the [`Identity`] of the root of the graph.
    fn root_identity(&self) -> Identity {
        self.graph.root()
    }

    /// Populate [`created_units_with_deps`] with [`Unit`]s and their dependencies.
    ///
    /// [`created_units_with_deps`]: Self::created_units_with_deps
    fn populate_units(&self) {
        for identity in self.id_to_identity.iter().copied() {
            let package = self
                .packages
                .borrow_mut()
                .remove(&identity)
                .unwrap_or_else(|| missing_depenendcy_in_graph(identity, &self.identity_to_id));
            let node = self.graph.dependencies_for_package(&identity);
            let unit = self.create_single_unit(identity, package);
            self.populate_unit_deps(&unit, node);
        }
    }

    /// Create a new [`Unit`].
    ///
    /// This will use [`next_available_id`] as the [`UnitId`] of this [`Unit`].
    ///
    /// This function will:
    /// * _always_ create a new [`Unit`],
    /// * put that new [`Unit`] into [`created_units_with_deps`] with an empty vector as
    ///   dependencies.
    ///
    /// [`next_available_id`]: Self::next_available_id
    /// [`created_units_with_deps`]: Self::created_units_with_deps
    fn create_single_unit(&self, unit_identity: Identity, package: CompilerPackage) -> Unit {
        let unit_type = infer_unit_type(unit_identity, self.root_identity());
        let unit_id = self.next_available_id();
        let unit = Unit::new(
            unit_id,
            package,
            unit_identity,
            unit_type,
            BuildKind::Compile,
        );
        let previous = self
            .created_units_with_deps
            .borrow_mut()
            .insert(unit.clone(), vec![]);
        debug_assert_eq!(
            previous, None,
            "we've inserted a new Unit, it shouldn't overwrite anything"
        );
        unit
    }

    /// Populate [`created_units_with_deps`] of the `unit` with its direct dependencies from `node`.
    ///
    /// [`created_units_with_deps`]: Self::created_units_with_deps
    fn populate_unit_deps(&self, unit: &Unit, node: &DependencyNode) {
        let mut map = self.created_units_with_deps.borrow_mut();
        let deps = map
            .get_mut(unit)
            .expect("`create_single_unit` always inserts unit in the map; we don't create units any other way");
        node.dependencies().iter().for_each(|dep| {
            deps.push(
                *self
                    .identity_to_id
                    .get(dep)
                    .unwrap_or_else(|| missing_depenendcy_in_graph(*dep, &self.identity_to_id)),
            );
        });
    }
}

/// Create a map of [`Identity`] -> [`UnitId`].
/// We have to do it __before__ creating any [`Unit`], since:
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

/// Infer an appropriate [`UnitType`].
fn infer_unit_type(unit_identity: Identity, root_identity: Identity) -> UnitType {
    let is_root = unit_identity == root_identity;
    if is_root {
        return UnitType::Binary;
    }
    UnitType::Dependency
}
