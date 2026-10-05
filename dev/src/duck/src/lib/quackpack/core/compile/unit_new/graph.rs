use std::collections::{HashMap, HashSet, VecDeque};
use std::convert::Infallible;
use std::ops::ControlFlow;

use crate::quackpack::core::compile::compiler_package::CompilerPackage;
use crate::quackpack::core::compile::early_graph::EarlyGraph;
use crate::quackpack::core::compile::unit_new::graph_visitor::{GraphVisitor, TryGraphVisitor};
use crate::quackpack::core::compile::unit_new::{Unit, UnitType};
use crate::quackpack::core::identity::Identity;

pub type GraphNodeId = u32;

pub enum CompilationInput {
    PackageSourceCode(Identity),
}

pub struct UnitGraphNode {
    id: GraphNodeId,
    node: UnitGraphNodeInner,
}

impl UnitGraphNode {
    fn new_unit(unit: Unit, id: GraphNodeId) -> Self {
        Self {
            id,
            node: UnitGraphNodeInner::Unit(unit),
        }
    }

    fn new_input(input: CompilationInput, id: GraphNodeId) -> Self {
        Self {
            id,
            node: UnitGraphNodeInner::Input(input),
        }
    }

    pub fn id(&self) -> GraphNodeId {
        self.id
    }

    pub fn as_unit(&self) -> Option<&Unit> {
        if let UnitGraphNodeInner::Unit(unit) = &self.node {
            Some(unit)
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

pub enum UnitGraphNodeInner {
    Unit(Unit),
    Input(CompilationInput),
}

pub struct UnitGraph {
    nodes: Vec<UnitGraphNode>,
    dependencies: Vec<Vec<GraphNodeId>>,
}

impl UnitGraph {
    pub fn node_for(&self, id: GraphNodeId) -> &UnitGraphNode {
        &self.nodes[id as usize]
    }

    pub fn deps_for(&self, id: GraphNodeId) -> &[GraphNodeId] {
        &self.dependencies[id as usize]
    }
}

pub(crate) struct UnitGraphBuilder {
    identity_to_nodes: HashMap<Identity, NodesForIdentity>,
    next_free_id: u32,
    nodes: Vec<UnitGraphNode>,
    dependencies: Vec<Vec<GraphNodeId>>,
}

#[derive(Default)]
struct NodesForIdentity {
    source_code_input: Option<GraphNodeId>,
    compile_dependency: Option<GraphNodeId>,
    compile_binary: Option<GraphNodeId>,
}

impl UnitGraphBuilder {
    pub fn new() -> Self {
        Self {
            identity_to_nodes: HashMap::new(),
            next_free_id: 1,
            nodes: vec![],
            dependencies: vec![],
        }
    }

    fn add_new_input(&mut self, input: CompilationInput) -> GraphNodeId {
        let id = self.next_free_id;
        self.next_free_id += 1;
        self.nodes.push(UnitGraphNode::new_input(input, id));
        // Inputs do not have any dependencies.
        self.dependencies.push(vec![]);
        id
    }

    fn add_new_unit(&mut self, unit: Unit, deps: Vec<GraphNodeId>) -> GraphNodeId {
        let id = self.next_free_id;
        self.next_free_id += 1;
        self.nodes.push(UnitGraphNode::new_unit(unit, id));
        self.dependencies.push(deps);
        id
    }

    fn nodes_for_identity(&mut self, identity: Identity) -> &mut NodesForIdentity {
        self.identity_to_nodes.entry(identity).or_default()
    }

    fn source_code_input(&mut self, identity: Identity) -> GraphNodeId {
        if let Some(result) = self.nodes_for_identity(identity).source_code_input {
            return result;
        }
        let new_node = self.add_new_input(CompilationInput::PackageSourceCode(identity));
        self.nodes_for_identity(identity).source_code_input = Some(new_node);
        new_node
    }

    pub fn add_compile_dependency_unit(
        &mut self,
        identity: Identity,
        package: CompilerPackage,
        all_deps: HashSet<Identity>,
    ) -> GraphNodeId {
        let src_deps = all_deps
            .into_iter()
            .map(|dep| self.source_code_input(dep))
            .collect();
        self.add_new_unit(Unit::new(package, identity, UnitType::Dependency), src_deps)
    }

    pub fn add_compile_binary_unit(
        &mut self,
        identity: Identity,
        package: CompilerPackage,
        all_deps: HashSet<Identity>,
    ) -> GraphNodeId {
        let src_deps: Vec<GraphNodeId> = all_deps
            .iter()
            .map(|dep| self.source_code_input(*dep))
            .collect();
        let compilation_deps: Vec<GraphNodeId> = all_deps
            .into_iter()
            .filter_map(|dep| {
                if dep != identity {
                    Some(
                        self.nodes_for_identity(dep)
                            .compile_dependency
                            .expect("todo"),
                    )
                } else {
                    None
                }
            })
            .collect();
        self.add_new_unit(
            Unit::new(package, identity, UnitType::Binary),
            [&src_deps[..], &compilation_deps[..]].concat(),
        )
    }

    pub fn build(self) -> UnitGraph {
        let Self {
            identity_to_nodes: _,
            next_free_id: _,
            nodes,
            dependencies,
        } = self;
        UnitGraph {
            nodes,
            dependencies,
        }
    }
}

pub fn lower_early_graph(graph: EarlyGraph) -> UnitGraph {
    let mut builder = UnitGraphBuilder::new();
    let (pkgs, graph) = graph.into_inner();
    for (identity, package) in pkgs.into_inner() {
        let all_package_deps = graph.reachable_subgraph_nodes(identity);
        if identity == graph.root() {
            builder.add_compile_binary_unit(identity, package, all_package_deps);
        }
    }
    builder.build()
}
