#pragma once

#include "node_id.hpp"

#include "base/bit256.hpp"
#include <base/maps.hpp>

#include <cstring>
#include <ostream>
#include <vector>

namespace query::detail {

	class QueryState;

	class QueryGraph {
		base::HashMap<NodeID, std::vector<NodeID>> node_deps;

		/**
		 * @brief Helper function to print nodes and their dependencies.
		 * @param nodes Vector of NodeIDs to print.
		 * @param out Output stream to print to.
		 */
		void debugPrintNodes(const std::vector<NodeID>& nodes, std::ostream& out) const;

		/*
		 * for direct acces to node_deps
		 */
		friend class QueryState;

	public:
		QueryGraph()                             = default;
		QueryGraph(const QueryGraph&)            = delete;
		QueryGraph(QueryGraph&&)                 = delete;
		QueryGraph& operator=(const QueryGraph&) = delete;
		QueryGraph& operator=(QueryGraph&&)      = delete;

		enum class DependencyStatus { OK, Cycle };

		/**
		 * @brief Marks that given query depends on another query.
		 * Note that @p to does not need to be in the graph at the moment of calling this function.
		 */
		DependencyStatus addDependency(detail::NodeID from, detail::NodeID to);

		/**
		 * Returns all dependencies of a @p node_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDeps(detail::NodeID node_id) const;

		/**
		 * Returns all dependencies of a @p node_id of type @p dependency_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDepsFiltered(detail::NodeID node_id, QueryID dependency_id) const;

		void debugPrint(std::ostream& out) const;
		void debugPrintForDrawing(std::ostream& out) const;

		/**
		 * @brief Returns all dependencies of a given query call.
		 */
		template<class Query>
		auto getNodeDeps(typename Query::QKey key) const {
			detail::NodeID node_id = makeNodeID(Query::getID(), key);
			return this->getNodeDeps(node_id);
		}

		/**
		 * @brief Returns all dependencies arising from @p dependency_id of a given query call.
		 */
		template<class Query>
		auto getNodeDepsFiltered(typename Query::QKey key, detail::QueryID dependency_id) const {
			detail::NodeID node_id = makeNodeID(Query::getID(), key);
			return this->getNodeDepsFiltered(node_id, dependency_id);
		}

		/**
		 * @brief Serializes the QueryGraph into a vector of bytes.
		 * @return A vector of bytes representing the serialized QueryGraph.
		 */
		[[nodiscard]] std::vector<uint8_t> serialize() const {
			std::vector<uint8_t> buffer;

			// Serialize the size of the node_deps map
			size_t map_size = node_deps.size();
			buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&map_size),
						  reinterpret_cast<const uint8_t*>(&map_size) + sizeof(map_size));

			// Serialize each entry in the map
			for (const auto& [node, deps] : node_deps) {
				// Serialize NodeID (q_id and hash)
				auto q_id = node.q_id.asInt();
				buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&q_id),
							  reinterpret_cast<const uint8_t*>(&q_id) + sizeof(q_id));

				auto hash_val = node.hash.val;
				buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&hash_val),
							  reinterpret_cast<const uint8_t*>(&hash_val) + sizeof(hash_val));

				// Serialize the dependencies vector size
				size_t deps_size = deps.size();
				buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&deps_size),
							  reinterpret_cast<const uint8_t*>(&deps_size) + sizeof(deps_size));

				// Serialize each dependency (NodeID)
				for (const auto& dep : deps) {
					auto dep_q_id = dep.q_id.asInt();
					buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&dep_q_id),
								  reinterpret_cast<const uint8_t*>(&dep_q_id) + sizeof(dep_q_id));

					auto dep_hash_val = dep.hash.val;
					buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&dep_hash_val),
								  reinterpret_cast<const uint8_t*>(&dep_hash_val) + sizeof(dep_hash_val));
				}
			}

			return buffer;
		}

		/**
		 * @brief Deserializes a QueryGraph from a vector of bytes.
		 * @param data The vector of bytes to deserialize from.
		 * @return A deserialized QueryGraph object.
		 */
		static QueryGraph deserialize(const std::vector<uint8_t>& data) {
			QueryGraph graph;
			size_t offset = 0;

			// Deserialize the size of the node_deps map
			size_t map_size;
			std::memcpy(&map_size, data.data() + offset, sizeof(map_size));
			offset += sizeof(map_size);

			// Deserialize each entry in the map
			for (size_t i = 0; i < map_size; ++i) {
				// Deserialize NodeID (q_id and hash)
				QueryID q_id;
				std::memcpy(&q_id, data.data() + offset, sizeof(q_id));
				offset += sizeof(q_id);

				base::Bit256 hash;
				std::memcpy(&hash, data.data() + offset, sizeof(hash.val));
				offset += sizeof(hash);

				NodeID node{.q_id=q_id, .hash={hash}};

				// Deserialize the dependencies vector size
				size_t deps_size;
				std::memcpy(&deps_size, data.data() + offset, sizeof(deps_size));
				offset += sizeof(deps_size);

				// Deserialize each dependency (NodeID)
				std::vector<NodeID> deps;
				for (size_t j = 0; j < deps_size; ++j) {
					QueryID dep_q_id;
					std::memcpy(&dep_q_id, data.data() + offset, sizeof(dep_q_id));
					offset += sizeof(dep_q_id);

					base::Bit256 dep_hash;
					std::memcpy(&dep_hash.val, data.data() + offset, sizeof(dep_hash.val));
					offset += sizeof(dep_hash.val);

					deps.emplace_back(NodeID{.q_id=dep_q_id, .hash=dep_hash});
				}

				// Add the deserialized entry to the graph
				graph.node_deps[node] = std::move(deps);
			}

			return graph;
		}

		~QueryGraph() = default;
	};
}
