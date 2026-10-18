// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use std::collections::{HashMap, HashSet};

use crate::quackpack::core::compile::unit::graph::GraphNodeId;
use crate::quackpack::core::compile::unit_runner::incremental::RedGreenData;
use crate::quackpack::core::compile::unit_runner::incremental::serializable_types::{
    SerializableGraphNode, SerializableGraphNodeData, SerializableUnitGraph,
};
use crate::quackpack::core::identity::Identity;
use crate::{QuackResult, qp_bail};

impl SerializableUnitGraph {
    /// Check if deserialized [`SerializableUnitGraph`] is not malformed.
    pub fn check_correctness(&self) -> QuackResult<()> {
        for (node_id, node) in self.nodes.iter().enumerate() {
            // Check that index in the vector is equal to `node_id`.
            if node_id as u32 != node.id {
                qp_bail!(
                    "node number {node_id} in the previous build unit graph has id {} instead of {node_id}",
                    node.id
                );
            }
            // Check that this unit's underlying package was in the graph.
            let pkg_identity = node.node.identity();
            if !self.packages.contains_key(&pkg_identity) {
                qp_bail!(
                    "node number {node_id} in the previous build unit graph points to a package {pkg_identity} but no such package was found in the graph"
                );
            }

            // Check node dependencies.
            // Firstly that they exist.
            let Some(node_deps) = self.dependencies.get(node_id) else {
                qp_bail!("no dependencies for node {node_id} in the previous build unit graph");
            };
            // Secondly that they are sorted.
            // This also checks that there are no duplicates, in contrast to `Vec::is_sorted` :).
            if !node_deps.is_sorted_by(|a, b| a < b) {
                qp_bail!(
                    "dependencies of the node {node_id} are not sorted in the previous build unit graph"
                );
            }
            // Lastly that they give topological order.
            if let Some(last) = node_deps.last()
                && *last >= node_id as u32
            {
                qp_bail!(
                    "nodes of the previous build graph are not topologically ordered: node {node_id} depends on node {last}"
                );
            }
        }
        Ok(())
    }

    /// Get information about which nodes and packages in the current graph are the same as in the previous graph.
    /// Creates [`RedGreenData`] with correct values of `common_packages` and `common_nodes`.
    /// `green_nodes` is left empty, to be gradually filled up by the compilation process.
    pub fn get_red_green_data(&self, other: &Self) -> RedGreenData {
        let common_packages = self.get_common_packages(other);
        let common_nodes = self.get_common_nodes(other, &common_packages);
        RedGreenData {
            common_packages,
            common_nodes,
            green_nodes: HashSet::new(),
        }
    }

    /// Helper for [`Self::get_red_green_data`].
    /// Checks which packages are the same (have the same [`Identity`] and [`SerializablePackageData`]) as in the previous build.
    /// This in fact checks that:
    /// - identities and versions are the same,
    /// - package localizations on the disk are the same,
    /// - features of the package are the same.
    fn get_common_packages(&self, other: &Self) -> HashSet<Identity> {
        let mut result = HashSet::new();
        for (identity, pkg_data) in self.packages.iter() {
            let Some(other_pkg_data) = other.packages.get(identity) else {
                continue;
            };
            if pkg_data == other_pkg_data {
                result.insert(*identity);
                continue;
            }
        }
        result
    }

    /// Helper for [`Self::get_red_green_data`].
    /// After [`Self::get_common_packages`] is run, checks which nodes are the same as in the previous graph.
    /// This not only means that they describe the same tasks on the same package, but that the dependencies are the same in both graphs.
    ///
    /// We achieve this, by iterating over the current graph in the topological order and maintaining the result.
    /// We add a node to the result if the tasks are the same and all the dependencies describe the same tasks and are already in the result.
    fn get_common_nodes(
        &self,
        other: &Self,
        common_packages: &HashSet<Identity>,
    ) -> HashSet<GraphNodeId> {
        let mut result = HashSet::new();
        let other_nodes: HashMap<&SerializableGraphNodeData, &SerializableGraphNode> =
            other.nodes.iter().map(|node| (&node.node, node)).collect();
        for node in self.nodes.iter() {
            let Some(other_node) = other_nodes.get(&node.node) else {
                continue;
            };
            if self.nodes_are_the_same(other, node, other_node, &result, common_packages) {
                result.insert(node.id);
            }
        }
        result
    }

    /// Given two seemingly equal graph nodes, checks that they are truly equal, meaning:
    /// - the package they both refer to has not changed in the meantime,
    /// - the dependencies of both nodes are the same.
    ///
    /// This function assumes that:
    /// ```rust,ignore (illustrative)
    /// self_node.node == other_node.node
    /// ```
    ///
    /// Important:
    /// ----------
    /// `current_common_nodes` should contain all nodes with id <= `self_node.id` which have their respective equals in the previous graph.
    /// Panics:
    /// -------
    /// May panic if any of the graphs is malformed (e.g. there are more nodes than dependencies vectors).
    /// If any of the graphs come from deserialization (usually `other`), they should be validated with [`Self::check_correctness`].
    fn nodes_are_the_same(
        &self,
        other: &Self,
        self_node: &SerializableGraphNode,
        other_node: &SerializableGraphNode,
        current_common_nodes: &HashSet<GraphNodeId>,
        common_packages: &HashSet<Identity>,
    ) -> bool {
        // The tasks and packages should be equal at this point.
        debug_assert_eq!(self_node.node, other_node.node);

        // If the package's data changed between compilations, return false.
        if !common_packages.contains(&self_node.node.identity()) {
            return false;
        }
        // Since the way `UnitGraphs` are built does not generally change,
        // if two nodes have the same dependencies, those dependencies should follow the same order.
        // So we check that the number of dependencies agree and that dependency at index `i` in the current graph
        // is the same as the dependency at index `i` in the previous graph.
        let self_node_deps = &self.dependencies[self_node.id as usize];
        let other_node_deps = &other.dependencies[other_node.id as usize];
        if self_node_deps.len() != other_node_deps.len() {
            return false;
        }
        self_node_deps
            .iter()
            .zip(other_node_deps)
            .all(|(self_dep, other_dep)| {
                self.nodes[*self_dep as usize].node == other.nodes[*other_dep as usize].node
                    && current_common_nodes.contains(self_dep)
            })
    }
}

#[cfg(test)]
mod test {
    use std::str::FromStr;

    use crate::quackpack::core::compile::unit_runner::incremental::RedGreenData;
    use crate::quackpack::core::compile::unit_runner::incremental::serializable_types::SerializableUnitGraph;
    use crate::quackpack::core::identity::Identity;

    /// Serialized graph for scenario "foo depends on both bar and baz".
    const LARGE_GRAPH: &'static str = r#"{
    "root-id": 5,
    "nodes": [
      {
        "id": 0,
        "node": {
          "input": {
            "package-source-code": "bar local+file:///home/bar"
          }
        }
      },
      {
        "id": 1,
        "node": {
          "input": {
            "package-source-code": "baz local+file:///home/baz"
          }
        }
      },
      {
        "id": 2,
        "node": {
          "input": {
            "package-source-code": "foo local+file:///home/foo"
          }
        }
      },
      {
        "id": 3,
        "node": {
          "unit": {
            "identity": "bar local+file:///home/bar",
            "unit-type": "Dependency"
          }
        }
      },
      {
        "id": 4,
        "node": {
          "unit": {
            "identity": "baz local+file:///home/baz",
            "unit-type": "Dependency"
          }
        }
      },
      {
        "id": 5,
        "node": {
          "unit": {
            "identity": "foo local+file:///home/foo",
            "unit-type": "Binary"
          }
        }
      }
    ],
    "dependencies": [
      [],
      [],
      [],
      [0],
      [1],
      [0,1,2,3,4]
    ],
    "packages": {
      "foo local+file:///home/foo": {
        "root-path": "/home/foo",
        "deps-realization": [
          [
            "bar local+file:///home/bar",
            null
          ],
          [
            "baz local+file:///home/baz",
            null
          ]
        ],
        "enabled-features": [],
        "identity": "foo local+file:///home/foo",
        "version": "1.0.0"
      },
      "bar local+file:///home/bar": {
        "root-path": "/home/bar",
        "deps-realization": [],
        "enabled-features": [],
        "identity": "bar local+file:///home/bar",
        "version": "1.0.0"
      },
      "baz local+file:///home/baz": {
        "root-path": "/home/baz",
        "deps-realization": [],
        "enabled-features": [],
        "identity": "baz local+file:///home/baz",
        "version": "1.0.0"
      }
    }
  }"#;

    /// Serialized graph for scenario "foo depends on bar".
    const SMALL_GRAPH: &'static str = r#"{
    "root-id": 3,
    "nodes": [
      {
        "id": 0,
        "node": {
          "input": {
            "package-source-code": "bar local+file:///home/bar"
          }
        }
      },
      {
        "id": 1,
        "node": {
          "input": {
            "package-source-code": "foo local+file:///home/foo"
          }
        }
      },
      {
        "id": 2,
        "node": {
          "unit": {
            "identity": "bar local+file:///home/bar",
            "unit-type": "Dependency"
          }
        }
      },
      {
        "id": 3,
        "node": {
          "unit": {
            "identity": "foo local+file:///home/foo",
            "unit-type": "Binary"
          }
        }
      }
    ],
    "dependencies": [
      [],
      [],
      [0],
      [0,1,2]
    ],
    "packages": {
      "foo local+file:///home/foo": {
        "root-path": "/home/foo",
        "deps-realization": [
          [
            "bar local+file:///home/bar",
            null
          ]
        ],
        "enabled-features": [],
        "identity": "foo local+file:///home/foo",
        "version": "1.0.0"
      },
      "bar local+file:///home/bar": {
        "root-path": "/home/bar",
        "deps-realization": [],
        "enabled-features": [],
        "identity": "bar local+file:///home/bar",
        "version": "1.0.0"
      }
    }
  }"#;

    /// As [`SMALL_GRAPH`], but dependencies are not sorted.
    const MALFORMED_GRAPH: &'static str = r#"{
    "root-id": 3,
    "nodes": [
      {
        "id": 0,
        "node": {
          "input": {
            "package-source-code": "bar local+file:///home/bar"
          }
        }
      },
      {
        "id": 1,
        "node": {
          "input": {
            "package-source-code": "foo local+file:///home/foo"
          }
        }
      },
      {
        "id": 2,
        "node": {
          "unit": {
            "identity": "bar local+file:///home/bar",
            "unit-type": "Dependency"
          }
        }
      },
      {
        "id": 3,
        "node": {
          "unit": {
            "identity": "foo local+file:///home/foo",
            "unit-type": "Binary"
          }
        }
      }
    ],
    "dependencies": [
      [],
      [],
      [0],
      [2,1,0]
    ],
    "packages": {
      "foo local+file:///home/foo": {
        "root-path": "/home/foo",
        "deps-realization": [
          [
            "bar local+file:///home/bar",
            null
          ]
        ],
        "enabled-features": [],
        "identity": "foo local+file:///home/foo",
        "version": "1.0.0"
      },
      "bar local+file:///home/bar": {
        "root-path": "/home/bar",
        "deps-realization": [],
        "enabled-features": [],
        "identity": "bar local+file:///home/bar",
        "version": "1.0.0"
      }
    }
  }"#;

    #[test]
    fn get_red_green_data_check() {
        let without_baz = serde_json::from_str::<SerializableUnitGraph>(SMALL_GRAPH).unwrap();
        let with_baz = serde_json::from_str::<SerializableUnitGraph>(LARGE_GRAPH).unwrap();
        let identity_bar = Identity::from_str("bar local+file:///home/bar").unwrap();
        let identity_baz = Identity::from_str("baz local+file:///home/baz").unwrap();
        let identity_foo = Identity::from_str("foo local+file:///home/foo").unwrap();
        let without_to_with = with_baz.get_red_green_data(&without_baz);
        let with_to_without = without_baz.get_red_green_data(&with_baz);
        let with_to_with = with_baz.get_red_green_data(&with_baz);
        assert_eq!(
            without_to_with,
            RedGreenData {
                common_packages: [identity_bar].into(),
                common_nodes: [0, 3].into(),
                green_nodes: [].into(),
            }
        );
        assert_eq!(
            with_to_without,
            RedGreenData {
                common_packages: [identity_bar].into(),
                common_nodes: [0, 2].into(),
                green_nodes: [].into(),
            }
        );
        assert_eq!(
            with_to_with,
            RedGreenData {
                common_packages: [identity_bar, identity_baz, identity_foo].into(),
                common_nodes: [0, 1, 2, 3, 4, 5].into(),
                green_nodes: [].into(),
            }
        );
    }

    #[test]
    fn errors_on_malformed_graph() {
        let graph = serde_json::from_str::<SerializableUnitGraph>(MALFORMED_GRAPH).unwrap();
        let err = graph.check_correctness().unwrap_err();
        assert_eq!(
            err.to_string(),
            "dependencies of the node 3 are not sorted in the previous build unit graph"
        )
    }
}
