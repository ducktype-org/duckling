//! A dependency graph implementation for QuackPack’s build system.

use std::collections::{BTreeMap, BTreeSet};
use std::hash::Hash;

type EquivalenceClass = usize;

#[derive(Debug)]
pub enum CompilationDecision<N> {
    Finished,
    More(Vec<N>),
}

#[derive(Debug)]
pub struct DependencyGraph<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>> {
    /// Created strongly connected components.
    ///
    /// Note that:
    /// * we use indices of the outer vector as equivalence classes when creating a reduced graph
    ///   (without cycles).
    sccs: Vec<BTreeSet<N>>,

    /// Topo-sorted compilation order.
    ///
    /// It's a topo sort order of quotients of the original graph with equivalence relation “nodes
    /// are in the same sccs.”
    compilation_order: Vec<EquivalenceClass>,

    /// Which nodes have been compiled.
    ///
    /// We need to track this separately so callers are allowed to each SCC by parts.
    compiled_nodes: BTreeMap<EquivalenceClass, BTreeSet<N>>,
}

impl<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>> DependencyGraph<N> {
    /// Construct a new [`DependencyGraph`].
    ///
    /// This will also precompute metadata for efficient compilation order.
    pub fn new(graph: &[Vec<N>]) -> Self {
        let sccs = kosaraju_sccs(graph);
        let reduced_graph = quotient_graph_from_sccs(&sccs, graph);
        let compilation_order = infer_compilation_order(&reduced_graph);
        Self {
            sccs,
            compilation_order,
            compiled_nodes: Default::default(),
        }
    }

    /// Get next nodes to compile.
    pub fn nodes_to_compile(&self) -> CompilationDecision<N> {
        let Some(scc_idx) = self.current_scc_idx() else {
            return CompilationDecision::Finished;
        };
        let scc: &BTreeSet<N> = &self.sccs[scc_idx];
        let mut to_compile = scc
            .iter()
            .filter(|node| !self.is_compiled(scc_idx, node))
            .copied()
            .collect::<Vec<_>>();
        to_compile.sort();
        to_compile.dedup();
        CompilationDecision::More(to_compile)
    }

    /// Get the current SCC index.
    fn current_scc_idx(&self) -> Option<EquivalenceClass> {
        self.compilation_order.last().copied()
    }

    /// Mark nodes as compiled.
    ///
    /// Note that if nodes shouldn't be compiled, they are ignored.
    pub fn mark_as_compiled(&mut self, compiled: impl IntoIterator<Item = N>) {
        let Some(scc_idx) = self.current_scc_idx() else {
            // We've compiled everything.
            return;
        };

        // Mark nodes as compiled.
        for node in compiled {
            let is_in_valid_scc = self.sccs[scc_idx].contains(&node);
            if !is_in_valid_scc {
                continue;
            }
            self.compiled_nodes.entry(scc_idx).or_default().insert(node);
        }

        // It might turn out that someone wanted to mark completely random nodes as compiled (and
        // `compiled_nodes` is still empty).
        let Some(compiled_nodes) = self.compiled_nodes.get(&scc_idx) else {
            return;
        };

        // If we've compiled entire SCC, move to the next one.
        let compiled_entire_scc = compiled_nodes.len() == self.sccs[scc_idx].len();
        if compiled_entire_scc {
            self.compilation_order.pop();
        }
    }

    /// Check if node of the given SCC is compiled.
    fn is_compiled(&self, scc_idx: EquivalenceClass, node: &N) -> bool {
        let Some(compiled) = self.compiled_nodes.get(&scc_idx) else {
            // We haven't touched this SCC.
            return false;
        };
        compiled.contains(node)
    }
}

/// Computes the decomposition of the graph into strongly connected components.
/// Returns a vector of vectors, each vector lists vertices in one stronly connected component.
fn kosaraju_sccs<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>>(
    graph: &[Vec<N>],
) -> Vec<BTreeSet<N>> {
    // This is the Kosaraju's algorithm for finding strongly connected components of a graph.
    // Step 1: First DFS to get finishing order
    let mut visited = BTreeSet::new();
    let mut order = Vec::new();

    for idx in 0..graph.len() {
        let vertex = N::from(idx);
        if !visited.contains(&vertex) {
            dfs1(graph, vertex, &mut visited, &mut order);
        }
    }

    // Step 2: Build reversed graph.
    let reversed = reverse_graph(graph);

    // Step 3: DFS on reversed graph in post-order of the first dfs.
    let mut visited = BTreeSet::new();
    let mut sccs = Vec::new();

    for vertex in order.iter().rev() {
        if !visited.contains(vertex) {
            let mut scc = BTreeSet::new();
            dfs2(&reversed, *vertex, &mut visited, &mut scc);
            sccs.push(scc);
        }
    }

    sccs
}

/// Helper for [`kosaraju_sccs`].
/// Forwards DFS pass.
fn dfs1<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>>(
    graph: &[Vec<N>],
    vertex: N,
    visited: &mut BTreeSet<N>,
    order: &mut Vec<N>,
) {
    visited.insert(vertex);

    let idx: usize = vertex.into();

    if let Some(neighbors) = graph.get(idx) {
        for neighbor in neighbors.iter().rev() {
            if !visited.contains(neighbor) {
                dfs1(graph, *neighbor, visited, order);
            }
        }
    }

    order.push(vertex);
}

/// Helper for [`kosaraju_sccs`].
/// Backwards DFS pass.
fn dfs2<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>>(
    graph: &BTreeMap<N, Vec<N>>,
    vertex: N,
    visited: &mut BTreeSet<N>,
    scc: &mut BTreeSet<N>,
) {
    visited.insert(vertex);
    scc.insert(vertex);

    if let Some(neighbors) = graph.get(&vertex) {
        for neighbor in neighbors {
            if !visited.contains(neighbor) {
                dfs2(graph, *neighbor, visited, scc);
            }
        }
    }
}

/// Helper for [`kosaraju_sccs`].
/// Computes the reversal of the dependencies graph.
fn reverse_graph<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>>(
    graph: &[Vec<N>],
) -> BTreeMap<N, Vec<N>> {
    let mut reversed = BTreeMap::new();

    // Initialize all vertices
    for idx in 0..graph.len() {
        let vertex = N::from(idx);
        reversed.insert(vertex, vec![]);
    }

    // Add reversed edges
    for (from, to_list) in graph.iter().enumerate() {
        let from = N::from(from);
        for to in to_list {
            reversed.entry(*to).or_default().push(from);
        }
    }

    reversed
}

/// Create a quotient/divided graph from SCCS and the original graph.
fn quotient_graph_from_sccs<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>>(
    sccs: &[BTreeSet<N>],
    graph: &[Vec<N>],
) -> BTreeMap<EquivalenceClass, BTreeSet<EquivalenceClass>> {
    let node_to_scc_index = reverse_sccs_mapping(sccs);
    let mut new_graph: BTreeMap<EquivalenceClass, BTreeSet<EquivalenceClass>> = BTreeMap::new();
    for (node, edges) in graph.iter().enumerate() {
        let node = N::from(node);
        let node_scc_index = node_to_scc_index[&node];
        // Ensure we register every SCC index. If a graph is a single cycle, equality check later
        // might produce an empty graph;
        new_graph.entry(node_scc_index).or_default();
        for edge in edges {
            let edge_scc_index = node_to_scc_index[edge];
            if edge_scc_index == node_scc_index {
                continue;
            }
            new_graph
                .entry(node_scc_index)
                .or_default()
                .insert(edge_scc_index);
        }
    }
    new_graph
}

/// We have mapping EquivalenceClass -> SCCS. Now we need to create a reverse mapping, N ->
/// SCC index.
fn reverse_sccs_mapping<N: Copy + Eq + Ord + Hash + From<usize> + Into<usize>>(
    sccs: &[BTreeSet<N>],
) -> BTreeMap<N, EquivalenceClass> {
    let mut result = BTreeMap::new();
    for (idx, scc) in sccs.iter().enumerate() {
        for node in scc {
            result.insert(*node, idx);
        }
    }
    result
}

/// Infer compilation order from computed sccs.
fn infer_compilation_order(
    graph: &BTreeMap<EquivalenceClass, BTreeSet<EquivalenceClass>>,
) -> Vec<EquivalenceClass> {
    fn infer_compilation_order_inner(
        graph: &BTreeMap<EquivalenceClass, BTreeSet<EquivalenceClass>>,
        idx: EquivalenceClass,
        order: &mut Vec<EquivalenceClass>,
        visited: &mut BTreeSet<EquivalenceClass>,
    ) {
        let inserted_new = visited.insert(idx);
        if !inserted_new {
            return;
        }
        let edges = &graph[&idx];
        // We want to preserve input order, so here we process in reverse.
        for edge in edges.iter().rev() {
            infer_compilation_order_inner(graph, *edge, order, visited);
        }
        order.insert(0, idx);
    }
    let mut topo_sort_order = Vec::new();
    let mut visited = BTreeSet::new();
    for idx in graph.keys() {
        infer_compilation_order_inner(graph, *idx, &mut topo_sort_order, &mut visited);
    }
    topo_sort_order
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeSet;

    type N = usize;

    fn make_sccs(groups: &[&[N]]) -> Vec<BTreeSet<N>> {
        groups
            .iter()
            .map(|group| group.iter().copied().collect())
            .collect()
    }

    fn bset(values: &[N]) -> BTreeSet<N> {
        values.iter().copied().collect()
    }

    fn assert_sccs_equal(actual: &[BTreeSet<N>], expected_groups: &[&[N]]) {
        let expected = make_sccs(expected_groups);
        assert_eq!(actual.len(), expected.len());
        for set in &expected {
            assert!(actual.contains(set), "SCC {:?} not found {:?}", set, actual);
        }
    }

    fn assert_topological_order(
        graph: &BTreeMap<EquivalenceClass, BTreeSet<EquivalenceClass>>,
        order: &[EquivalenceClass],
    ) {
        assert_eq!(
            graph.len(),
            order.len(),
            "all graph vertices should occur exactly once"
        );

        let positions: BTreeMap<_, _> = order
            .iter()
            .enumerate()
            .map(|(position, node)| (*node, position))
            .collect();

        assert_eq!(positions.len(), order.len());

        for (from, edges) in graph {
            for to in edges {
                assert!(
                    positions[from] < positions[to],
                    "edge {from} -> {to} violates topological order: {order:?}"
                );
            }
        }
    }

    #[test]
    fn kosaraju_sccs_empty_graph() {
        let graph: Vec<Vec<N>> = vec![];

        let sccs = kosaraju_sccs(&graph);

        assert!(sccs.is_empty());
    }

    #[test]
    fn kosaraju_sccs_simple() {
        // 0 -> 1 -> 2
        let graph: Vec<Vec<N>> = vec![vec![1], vec![2], vec![]];
        let sccs = kosaraju_sccs(&graph);

        assert_sccs_equal(&sccs, &[&[0], &[1], &[2]]);
    }

    #[test]
    fn kosaraju_sccs_cycle() {
        // 0 -> 1 -> 2 -> 0
        let graph: Vec<Vec<N>> = vec![vec![1], vec![2], vec![0]];
        let sccs = kosaraju_sccs(&graph);

        assert_sccs_equal(&sccs, &[&[0, 1, 2]]);
    }

    #[test]
    fn kosaraju_sccs_complex_cycles() {
        // Component A: 0 <-> 1
        // Component B: 2 <-> 3
        // Edge from 1 -> 2
        let graph: Vec<Vec<N>> = vec![
            vec![1],    // 0 -> 1
            vec![0, 2], // 1 -> 0, 1 -> 2
            vec![3],    // 2 -> 3
            vec![2],    // 3 -> 2
        ];
        let sccs = kosaraju_sccs(&graph);

        assert_sccs_equal(&sccs, &[&[0, 1], &[2, 3]]);
    }

    #[test]
    fn kosaraju_sccs_cycle_with_outgoing_edges() {
        // 0 -> 1 -> 2 -> 0
        // 2 -> 3
        // 3 -> 4
        let graph = vec![vec![1], vec![2], vec![0, 3], vec![4], vec![]];

        let sccs = kosaraju_sccs(&graph);

        assert_sccs_equal(&sccs, &[&[0, 1, 2], &[3], &[4]]);
    }

    #[test]
    fn kosaraju_sccs_self_loop() {
        let graph = vec![vec![0], vec![]];

        let sccs = kosaraju_sccs(&graph);

        assert_sccs_equal(&sccs, &[&[0], &[1]]);
    }

    #[test]
    fn quotient_graph_empty() {
        let graph: Vec<Vec<N>> = vec![];
        let sccs: Vec<BTreeSet<N>> = vec![];

        let quotient = quotient_graph_from_sccs(&sccs, &graph);

        assert!(quotient.is_empty());
    }

    #[test]
    fn quotient_graph_single_scc() {
        // 0 -> 1 -> 0
        let graph: Vec<Vec<N>> = vec![vec![1], vec![0]];
        let sccs = kosaraju_sccs(&graph);

        let quotient = quotient_graph_from_sccs(&sccs, &graph);

        assert_eq!(quotient.len(), 1);
        let scc_idx = 0;
        assert!(quotient.get(&scc_idx).unwrap().is_empty());
    }

    #[test]
    fn quotient_graph_multi_scc_dependencies() {
        // SCC 0: {0, 1}, SCC 1: {2}
        // Edge 1 -> 2 introduces a relation between SCCS
        let sccs: Vec<BTreeSet<N>> = make_sccs(&[&[0, 1], &[2]]);
        let graph: Vec<Vec<N>> = vec![
            vec![1],    // 0 -> 1
            vec![0, 2], // 1 -> 0, 1 -> 2
            vec![],     // 2
        ];

        let quotient = quotient_graph_from_sccs(&sccs, &graph);

        let scc_0_idx = sccs.iter().position(|s| s.contains(&0)).unwrap();
        let scc_1_idx = sccs.iter().position(|s| s.contains(&2)).unwrap();

        assert_eq!(
            quotient.get(&scc_0_idx).unwrap(),
            &BTreeSet::from([scc_1_idx])
        );
        assert!(quotient.get(&scc_1_idx).unwrap().is_empty());
    }

    #[test]
    fn quotient_graph_preserves_edges_between_sccs() {
        // SCC 0: {0}
        // SCC 1: {1}
        // SCC 2: {2}
        //
        // 0 -> 1
        // 0 -> 2
        // 1 -> 2
        let graph = vec![vec![1, 2], vec![2], vec![]];

        let sccs = vec![bset(&[0]), bset(&[1]), bset(&[2])];

        let quotient = quotient_graph_from_sccs(&sccs, &graph);

        let expected = BTreeMap::from([(0, bset(&[1, 2])), (1, bset(&[2])), (2, bset(&[]))]);

        assert_eq!(quotient, expected);
    }

    #[test]
    fn infer_compilation_order_empty() {
        let graph = BTreeMap::new();

        let order = infer_compilation_order(&graph);

        assert!(order.is_empty());
    }

    #[test]
    fn infer_compilation_order_linear() {
        // 0 -> 1 -> 2
        let mut quotient: BTreeMap<EquivalenceClass, BTreeSet<EquivalenceClass>> = BTreeMap::new();
        quotient.insert(0, BTreeSet::from([1]));
        quotient.insert(1, BTreeSet::from([2]));
        quotient.insert(2, BTreeSet::default());

        let order = infer_compilation_order(&quotient);

        assert_eq!(order, vec![0, 1, 2]);
    }

    #[test]
    fn infer_compilation_order_branching_graph() {
        //     0
        //    / \
        //   1   2
        //    \ /
        //     3
        let graph = BTreeMap::from([
            (0, bset(&[1, 2])),
            (1, bset(&[3])),
            (2, bset(&[3])),
            (3, bset(&[])),
        ]);

        let order = infer_compilation_order(&graph);

        assert_topological_order(&graph, &order);

        let position = |node| order.iter().position(|n| *n == node).unwrap();

        assert!(position(0) < position(1));
        assert!(position(0) < position(2));
        assert!(position(0) < position(3));

        assert!(position(1) < position(2));
        assert!(position(1) < position(3));

        assert!(position(2) < position(3));
    }

    #[test]
    fn dependency_graph_empty() {
        let graph: Vec<Vec<N>> = vec![];
        let dep_graph = DependencyGraph::<N>::new(&graph);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::Finished => {}
            CompilationDecision::More(nodes) => panic!("expected finished, not {:?}", nodes),
        }
    }

    #[test]
    fn dependency_graph_linear_pipeline() {
        // 0 -> 1 -> 2
        // Order of compilation: 2 -> 1 -> 0.
        let graph: Vec<Vec<N>> = vec![vec![1], vec![2], vec![]];
        let mut dep_graph = DependencyGraph::<N>::new(&graph);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![2]),
            CompilationDecision::Finished => panic!("expected 2"),
        }
        dep_graph.mark_as_compiled([2]);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![1]),
            CompilationDecision::Finished => panic!("expected 1"),
        }
        dep_graph.mark_as_compiled([1]);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![0]),
            CompilationDecision::Finished => panic!("expected 0"),
        }
        dep_graph.mark_as_compiled([0]);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::Finished => {}
            CompilationDecision::More(nodes) => panic!("expected finished, not {:?}", nodes),
        }
    }

    #[test]
    fn dependency_graph_cyclic_partial_compilation() {
        // 0 <-> 1
        let graph: Vec<Vec<N>> = vec![vec![1], vec![0]];
        let mut dep_graph = DependencyGraph::<N>::new(&graph);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![0, 1]),
            CompilationDecision::Finished => panic!("expected [0, 1]"),
        }

        dep_graph.mark_as_compiled([0]);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![1]),
            CompilationDecision::Finished => panic!("expected 1"),
        }

        dep_graph.mark_as_compiled([1]);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::Finished => {}
            CompilationDecision::More(nodes) => panic!("expected finished, not {:?}", nodes),
        }
    }

    #[test]
    fn dependency_graph_ignores_invalid_and_out_of_scc_nodes() {
        // 0 -> 1
        let graph: Vec<Vec<N>> = vec![vec![1], vec![]];
        let mut dep_graph = DependencyGraph::<N>::new(&graph);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![1]),
            _ => panic!("expected 1"),
        }

        // Should be ignored
        dep_graph.mark_as_compiled([0, 99, 99]);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![1]),
            _ => panic!("expected 1"),
        }

        dep_graph.mark_as_compiled([1]);

        match dep_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![0]),
            _ => panic!("expected 0"),
        }
    }

    #[test]
    fn dependency_graph_single_node() {
        let graph = vec![vec![]];

        let mut dependency_graph = DependencyGraph::new(&graph);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![0]),
            CompilationDecision::Finished => panic!("expected 0"),
        }

        dependency_graph.mark_as_compiled([0]);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::Finished => {}
            CompilationDecision::More(nodes) => panic!("expected finished, not {:?}", nodes),
        }
    }

    #[test]
    fn dependency_graph_deterministic_order() {
        //     0
        //    / \
        //   1   2
        //    \ /
        //     3
        let graph = vec![vec![1, 2], vec![3], vec![3], vec![]];
        let mut dependency_graph = DependencyGraph::new(&graph);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![3]),
            CompilationDecision::Finished => panic!("expected 3"),
        }

        dependency_graph.mark_as_compiled([3]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![2]),
            CompilationDecision::Finished => panic!("expected 2"),
        }

        dependency_graph.mark_as_compiled([2]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![1]),
            CompilationDecision::Finished => panic!("expected 1"),
        }

        dependency_graph.mark_as_compiled([1]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![0]),
            CompilationDecision::Finished => panic!("expected 0"),
        }

        dependency_graph.mark_as_compiled([0]);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::Finished => {}
            CompilationDecision::More(nodes) => panic!("expected finished, not {:?}", nodes),
        }
    }

    #[test]
    fn dependency_graph_deterministic_order2() {
        //     0
        //    / \  \
        //   1   2  3
        let graph = vec![vec![1, 2, 3], vec![], vec![], vec![]];
        let mut dependency_graph = DependencyGraph::new(&graph);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![3]),
            CompilationDecision::Finished => panic!("expected 3"),
        }

        dependency_graph.mark_as_compiled([3]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![2]),
            CompilationDecision::Finished => panic!("expected 2"),
        }

        dependency_graph.mark_as_compiled([2]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![1]),
            CompilationDecision::Finished => panic!("expected 1"),
        }

        dependency_graph.mark_as_compiled([1]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![0]),
            CompilationDecision::Finished => panic!("expected 0"),
        }

        dependency_graph.mark_as_compiled([0]);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::Finished => {}
            CompilationDecision::More(nodes) => panic!("expected finished, not {:?}", nodes),
        }
    }

    #[test]
    fn dependency_graph_deterministic_order3() {
        //     0
        //    / \
        //   1   2
        //    \ /
        //     3, 4
        let graph = vec![vec![1, 2], vec![3, 4], vec![3, 4], vec![], vec![]];
        let mut dependency_graph = DependencyGraph::new(&graph);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![4]),
            CompilationDecision::Finished => panic!("expected 4"),
        }

        dependency_graph.mark_as_compiled([4]);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![3]),
            CompilationDecision::Finished => panic!("expected 3"),
        }

        dependency_graph.mark_as_compiled([3]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![2]),
            CompilationDecision::Finished => panic!("expected 2"),
        }

        dependency_graph.mark_as_compiled([2]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![1]),
            CompilationDecision::Finished => panic!("expected 1"),
        }

        dependency_graph.mark_as_compiled([1]);
        match dependency_graph.nodes_to_compile() {
            CompilationDecision::More(nodes) => assert_eq!(nodes, vec![0]),
            CompilationDecision::Finished => panic!("expected 0"),
        }

        dependency_graph.mark_as_compiled([0]);

        match dependency_graph.nodes_to_compile() {
            CompilationDecision::Finished => {}
            CompilationDecision::More(nodes) => panic!("expected finished, not {:?}", nodes),
        }
    }
}
