// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::cell::OnceCell;
use std::collections::{HashMap, HashSet, VecDeque};
use std::convert::Infallible;
use std::ops::ControlFlow;
use std::sync::Arc;

use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::early_graph::{DependencyGraph, EarlyGraph};
use crate::quackpack::core::compile::unit::graph_visitor::{GraphVisitor, TryGraphVisitor};
use crate::quackpack::core::compile::unit::{PackageData, Unit, UnitType};
use crate::quackpack::core::identity::Identity;
use crate::quackpack::core::valid_package_name::normalise_package_name;
use crate::{QuackResult, QuackResultContext};

/// Graph encapsulating dependencies between different tasks to be done in the compilation workflow.
/// It has two types of nodes:
/// - [`Unit`]s, which are tasks to be done;
/// - [`CompilationInput`]s, which abstract over the inputs of the compilation workflow, namely source codes of packages.
///
/// Units can depend on both other units and inputs, while inputs have no dependencies.
///
/// The idea is for the [`UnitGraph`] to be a direct and truthful representation of the compilation workflow.
/// Edges directly reflect dependence at the level of units, not packages.
/// In particular [`UnitGraph`] is always acyclic.
///
/// Note:
/// -----
/// For [`UnitGraph`]s built with [`UnitGraphBuilder`], the identifiers constituate a topological order.
#[derive(Clone, Debug)]
pub struct UnitGraph {
    /// Identifier of the root unit, which is the main compilation goal.
    root_id: GraphNodeId,
    /// Nodes of the graph, ordered by [`GraphNodeId`].
    nodes: Vec<UnitGraphNode>,
    /// Dependencies of the nodes of the graph, ordered by [`GraphNodeId`].
    /// At index `i` we have dependencies of the [`UnitGraphNode`] with [`GraphNodeId`] == i.
    /// Recall that only units can have dependencies, so for inputs the relevant vector is empty.
    ///
    /// The whole vector has the same length as [`nodes`], which is guaranteed by [`UnitGraphBuilder`].
    ///
    /// [`nodes`]: Self::nodes
    dependencies: Vec<Vec<GraphNodeId>>,
    /// Information about packages used in this compilation workflow.
    packages: HashMap<Identity, Arc<PackageData>>,
}

impl UnitGraph {
    /// Get the [`UnitGraphNode`] for the given [`GraphNodeId`].
    pub fn node_for(&self, id: GraphNodeId) -> &UnitGraphNode {
        &self.nodes[id as usize]
    }

    /// Get the dependencies of the node specified by [`GraphNodeId`].
    pub fn deps_for(&self, id: GraphNodeId) -> &[GraphNodeId] {
        &self.dependencies[id as usize]
    }

    /// Get the root [`Unit`] of this graph.
    /// Panics:
    /// -------
    /// Panics when root node is not a unit.
    /// This cannot be reached through the [`UnitGraphBuilder`] interface.
    pub fn root_unit(&self) -> &Unit {
        self.node_for(self.root_id)
            .as_unit()
            .expect("root node is always a unit")
    }

    /// Get the [`GraphNodeId`] of the root node of this graph.
    pub fn root_id(&self) -> GraphNodeId {
        self.root_id
    }

    /// Check if a given [`Unit`] is the root node of this graph.
    pub fn is_root(&self, unit: &Unit) -> bool {
        self.root_unit() == unit
    }

    /// Get the [`PackageData`] for a package specified by an [`Identity`].
    pub fn package_data(&self, identity: Identity) -> &PackageData {
        self.packages
            .get(&identity)
            .unwrap_or_else(|| panic!("no package data for {identity}"))
    }

    /// Get the compilation order (topological order of the nodes of the graph).
    /// We assume that the order given by [`GraphNodeId`]s is a topological order.
    /// This is true for [`UnitGraph`]s constructed by the [`UnitGraphBuilder`] API.
    pub fn compilation_order(&self) -> &[UnitGraphNode] {
        &self.nodes
    }
}

/// A node in [`UnitGraph`].
#[derive(Clone, Debug)]
pub struct UnitGraphNode {
    /// Unique identifier of the node in the graph.
    id: GraphNodeId,
    node: UnitGraphNodeData,
}

#[derive(Clone, Debug)]
enum UnitGraphNodeData {
    Unit(Unit),
    Input(CompilationInput),
}

/// Unique identificator of [`UnitGraph`] nodes.
pub type GraphNodeId = u32;

/// Input on which [`Unit`] can depend.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum CompilationInput {
    /// Source code of a package.
    PackageSourceCode(Identity),
}

impl UnitGraphNode {
    /// Create a new [`UnitGraphNode`] which is a [`Unit`].
    fn new_unit(unit: Unit, id: GraphNodeId) -> Self {
        Self {
            id,
            node: UnitGraphNodeData::Unit(unit),
        }
    }

    /// Create a new [`UnitGraphNode`] which is a [`CompilationInput`].
    fn new_input(input: CompilationInput, id: GraphNodeId) -> Self {
        Self {
            id,
            node: UnitGraphNodeData::Input(input),
        }
    }

    /// Get the identifier of this node in the graph.
    pub fn id(&self) -> GraphNodeId {
        self.id
    }

    /// Unpack this [`UnitGraphNode`] if it is a [`Unit`].
    pub fn as_unit(&self) -> Option<&Unit> {
        if let UnitGraphNodeData::Unit(unit) = &self.node {
            Some(unit)
        } else {
            None
        }
    }

    /// Unpack this [`UnitGraphNode`] if it is a [`CompilationInput`].
    pub fn as_input(&self) -> Option<&CompilationInput> {
        if let UnitGraphNodeData::Input(input) = &self.node {
            Some(input)
        } else {
            None
        }
    }

    /// Accept a [`GraphVisitor`].
    ///
    /// This method should only drive the visitor through dependencies of this [`Unit`].
    ///
    /// If visitor returns `ControlFlow::Break(b)`, we short circuit to `Some(b)`.
    ///
    /// Otherwise (no breaks), we return `None`.
    pub fn accept<V: GraphVisitor + ?Sized>(
        &self,
        visitor: &mut V,
        graph: &UnitGraph,
    ) -> Option<V::Break> {
        struct VisitorAsTryVisitor<'a, U: ?Sized> {
            visitor: &'a mut U,
        }
        impl<U: GraphVisitor + ?Sized> TryGraphVisitor for VisitorAsTryVisitor<'_, U> {
            type Err = Infallible;

            type Break = U::Break;

            fn try_visit(
                &mut self,
                node: &UnitGraphNode,
            ) -> Result<ControlFlow<Self::Break>, Self::Err> {
                Ok(self.visitor.visit(node))
            }
        }
        let result = self.try_accept(&mut VisitorAsTryVisitor { visitor }, graph);
        let Ok(result) = result;
        result
    }

    /// Accept a [`TryGraphVisitor`].
    ///
    /// This method should only drive the visitor through dependencies of this [`Unit`].
    ///
    /// If visitor returns an `Err(e)`, we short circuit to `Err(e)`
    ///
    /// If it returns `Ok(ControlFlow::Break(b))`, we short circuit to `Ok(Some(b))`.
    ///
    /// Otherwise (no errors + no breaks), we return `Ok(None)`.
    pub fn try_accept<V: TryGraphVisitor + ?Sized>(
        &self,
        visitor: &mut V,
        graph: &UnitGraph,
    ) -> Result<Option<V::Break>, V::Err> {
        let mut stack = VecDeque::from([self.id()]);
        let mut visited = HashSet::new();
        while let Some(id) = stack.pop_front() {
            if visited.contains(&id) {
                continue;
            }
            visited.insert(id);
            let node = graph.node_for(id);
            if let ControlFlow::Break(b) = visitor.try_visit(node)? {
                return Ok(Some(b));
            }
            stack.extend(graph.deps_for(node.id()));
        }
        Ok(None)
    }
}

/// A struct which should be used for creating instances of [`UnitGraph`].
///
/// Note:
/// -----
/// This struct currently has an API which throws an internal error if someone tries to add a [`Unit`] and some of its dependencies is not yet present.
/// The other solution would be to add those dependencies units on the fly.
/// The issue with that approach is cycle-detection.
/// Adding build-scritps could introduce erronous situations (like two packages with build scripts depending on one another).
/// In such case it is impossible to construct the [`UnitGraph`] and
/// while it would be possible to detect such situations with the second aproach, it would be challenging to provide the user with proper diagnosis.
///
/// Thus it is the callers responsibility to deal with such situations as described above and create all the nodes in the right order.
///
/// API:
/// ----
/// `pub(crate)` indicates API to be used in [`lower_early_graph`], other functions are helpers.
pub(crate) struct UnitGraphBuilder {
    root: OnceCell<GraphNodeId>,
    identity_to_nodes: HashMap<Identity, NodesForIdentity>,
    identity_to_pkg_data: HashMap<Identity, Arc<PackageData>>,
    next_free_id: u32,
    nodes: Vec<UnitGraphNode>,
    dependencies: Vec<Vec<GraphNodeId>>,
}

/// A struct storing information of all units associated with a given package.
#[derive(Default)]
struct NodesForIdentity {
    source_code_input: Option<GraphNodeId>,
    compile_dependency: Option<GraphNodeId>,
    compile_binary: Option<GraphNodeId>,
}

impl UnitGraphBuilder {
    /// Create a new empty [`UnitGraphBuilder`].
    pub(crate) fn new() -> Self {
        Self {
            root: OnceCell::new(),
            identity_to_nodes: HashMap::new(),
            identity_to_pkg_data: HashMap::new(),
            next_free_id: 0,
            nodes: vec![],
            dependencies: vec![],
        }
    }

    /// Add a new [`CompilationInput`] node to the graph.
    fn add_new_input(&mut self, input: CompilationInput) -> GraphNodeId {
        let id = self.next_free_id;
        self.next_free_id += 1;
        self.nodes.push(UnitGraphNode::new_input(input, id));
        // Inputs do not have any dependencies.
        self.dependencies.push(vec![]);
        id
    }

    /// Add a new [`Unit`] node to the graph.
    fn add_new_unit(&mut self, unit: Unit, deps: Vec<GraphNodeId>) -> GraphNodeId {
        let id = self.next_free_id;
        self.next_free_id += 1;
        self.nodes.push(UnitGraphNode::new_unit(unit, id));
        self.dependencies.push(deps);
        id
    }

    /// Get a mutable access to the [`NodesForIdentity`] associated with the given [`Identity`].
    fn nodes_for_identity_mut(&mut self, identity: Identity) -> &mut NodesForIdentity {
        self.identity_to_nodes.entry(identity).or_default()
    }

    /// Get [`PackageData`] assocatied with a given [`Identity`].
    fn get_package_data(&self, identity: Identity) -> Arc<PackageData> {
        self.identity_to_pkg_data
            .get(&identity)
            .expect("no data for package")
            .clone()
    }

    /// Add [`PackageData`] of a package to the graph.
    pub(crate) fn add_package(&mut self, identity: Identity, package: Arc<PackageData>) {
        self.identity_to_pkg_data.insert(identity, package);
    }

    /// Add to the graph an input of package's source code.
    pub(crate) fn add_source_code_input(&mut self, identity: Identity) -> GraphNodeId {
        let new_node = self.add_new_input(CompilationInput::PackageSourceCode(identity));
        self.nodes_for_identity_mut(identity).source_code_input = Some(new_node);
        new_node
    }

    /// Ads to the graph a unit for compiling a package to dependency artifacts.
    /// Errors:
    /// -------
    /// Returns internal error if any of this unit's dependencies (namely source-code inputs) were not added to the graph beforehand.
    pub(crate) fn add_compile_dependency_unit(
        &mut self,
        identity: Identity,
        all_deps: HashSet<Identity>,
    ) -> QuackResult<GraphNodeId> {
        let mut src_deps = all_deps
            .into_iter()
            .map(|dep| {
                self.nodes_for_identity_mut(dep)
                    .source_code_input
                    .with_context_internal(|| format!("no source code input for {dep}"))
            })
            .collect::<QuackResult<Vec<GraphNodeId>>>()?;
        src_deps.sort();
        let new_node = self.add_new_unit(
            Unit::new(self.get_package_data(identity), UnitType::Dependency),
            src_deps,
        );
        self.nodes_for_identity_mut(identity).compile_dependency = Some(new_node);
        Ok(new_node)
    }

    /// Add to the graph a unit for compiling a package to binary artifacts.
    /// Errors:
    /// -------
    /// Returns internal error if any of this unit's dependencies (namely source-code inputs and compilation to dependency artifacts units)
    /// were not added to the graph beforehand.
    pub(crate) fn add_compile_binary_unit(
        &mut self,
        identity: Identity,
        all_deps: HashSet<Identity>,
    ) -> QuackResult<GraphNodeId> {
        let src_deps: Vec<GraphNodeId> = all_deps
            .iter()
            .map(|dep| {
                self.nodes_for_identity_mut(*dep)
                    .source_code_input
                    .with_context_internal(|| format!("no source code input for {dep}"))
            })
            .collect::<QuackResult<Vec<GraphNodeId>>>()?;
        let compilation_deps: Vec<GraphNodeId> = all_deps
            .into_iter()
            .filter_map(|dep| {
                if dep != identity {
                    Some(
                        self.nodes_for_identity_mut(dep)
                            .compile_dependency
                            .with_context_internal(|| {
                                format!("no compile dependency unit for {dep}")
                            }),
                    )
                } else {
                    None
                }
            })
            .collect::<QuackResult<Vec<GraphNodeId>>>()?;
        let mut all_deps = [&src_deps[..], &compilation_deps[..]].concat();
        all_deps.sort();
        let root_id = self.add_new_unit(
            Unit::new(self.get_package_data(identity), UnitType::Binary),
            all_deps,
        );
        self.root
            .set(root_id)
            .expect("UnitGraphBuilder supports only one root package");
        self.nodes_for_identity_mut(identity).compile_binary = Some(root_id);
        Ok(root_id)
    }

    /// Finish constructing the [`UnitGraph`].
    pub(crate) fn build(self) -> UnitGraph {
        let Self {
            mut root,
            identity_to_nodes: _,
            identity_to_pkg_data,
            next_free_id: _,
            nodes,
            dependencies,
        } = self;
        UnitGraph {
            root_id: root.take().expect("no root unit added to the graph"),
            nodes,
            dependencies,
            packages: identity_to_pkg_data,
        }
    }
}

/// Create a [`UnitGraph`] from [`EarlyGraph`], using the [`UnitGraphBuilder`] API.
pub fn lower_early_graph(graph: EarlyGraph) -> QuackResult<UnitGraph> {
    let mut builder = UnitGraphBuilder::new();
    let (pkgs, graph) = graph.into_inner();
    let mut pkgs = pkgs.into_inner();
    let mut sorted_identities: Vec<Identity> = graph
        .reachable_subgraph_nodes(graph.root())
        .into_iter()
        .collect();
    sorted_identities.sort();

    for identity in sorted_identities.iter() {
        let package = pkgs.remove(identity).expect("impossible");
        let pkg_data = Arc::new(construct_package_data(*identity, package, &graph));
        builder.add_package(*identity, pkg_data);
        builder.add_source_code_input(*identity);
    }

    for identity in sorted_identities {
        if identity != graph.root() {
            let all_package_deps = graph.reachable_subgraph_nodes(identity);
            builder.add_compile_dependency_unit(identity, all_package_deps)?;
        }
    }
    let root_deps = graph.reachable_subgraph_nodes(graph.root());
    builder.add_compile_binary_unit(graph.root(), root_deps)?;
    Ok(builder.build())
}

/// Construct [`PackageData`] from [`CompilerPackage`].
fn construct_package_data(
    identity: Identity,
    package: CompilerPackage,
    graph: &DependencyGraph,
) -> PackageData {
    let (package, features, _) = package.decompose();
    let direct_pkg_deps = graph.dependencies_for_package(&identity).dependencies();
    let direct_pkg_deps = direct_pkg_deps
        .iter()
        .map(|dep| {
            let alias = package
                .manifest()
                .dependencies()
                .get_by_name(dep.name())
                .unwrap_or_else(|| panic!("{identity} has no dependency named {}", dep.name()))
                .alias()
                .map(|alias| normalise_package_name(&alias).into());
            (*dep, alias)
        })
        .collect();
    PackageData {
        package,
        deps_realization: direct_pkg_deps,
        enabled_features: features,
        identity,
    }
}
