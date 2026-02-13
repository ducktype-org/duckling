#pragma once

#include "node_id.hpp"
#include "node_making.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/locks/assert_lock.hpp>

#include <base/collections/maps.hpp>
#include <base/config/build_type.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>
#include <base/preproc/utils.hpp>

#include <ostream>
#include <type_traits>
#include <vector>

namespace query::internal {

	class QueryState;

	/**
	 * @brief Core dependency graph powering evaluation across the compiler.
	 * \parallel Must be thread-safe as foundational infrastructure; all query categories assume this.
	 */
	class QueryGraph final {
		struct ChildrenData;
		template<class T>
		struct ChildrenDataHolderImpl;
		using ChildrenDataHolder      = ChildrenDataHolderImpl<ChildrenData>;
		using ConstChildrenDataHolder = ChildrenDataHolderImpl<const ChildrenData>;

		struct ChildrenData final {
		private:
			std::vector<NodeID> children;
			IF_BUILD_TYPE_DEV(mutable base::Box<concurrent::AssertLock> lock
			                  = base::makeBox<concurrent::AssertLock>();  // protects children vector
			)

		public:
			ChildrenData() = default;

			ChildrenData(const ChildrenData&) = delete;
			ChildrenData(ChildrenData&&)      = default;

			ChildrenData& operator=(const ChildrenData&) = delete;
			ChildrenData& operator=(ChildrenData&&)      = default;

			ChildrenData(std::vector<NodeID>&& children): children(std::move(children)) {}

			template<class T>
			friend struct ChildrenDataHolderImpl;

			/**
			 * @brief Get a holder for the children vector. The holder will lock the children data
			 * until it is destroyed.
			 * @return A holder for the children vector.
			 * @note Holder uses asert lock, so if two threads try to get the holder at the same
			 * time, one of them will panic. This is intentional, as it should never happen that two
			 * threads try to access the same node's children at the same time.
			 */
			ChildrenDataHolder getHolder() { return { this }; }

			[[nodiscard]]
			ConstChildrenDataHolder getHolder() const {
				return { this };
			}

			// Each node (query call with unique key) should be executed once at the same time, but
			// we use AssertLock to be sure about that
		};

		/**
		 * @brief Holder for the children data. Locks the children data until destroyed or release()
		 * is called.
		 *
		 * @note This is a template to allow for both const and non-const access to the children data.
		 */
		template<class T>
		struct ChildrenDataHolderImpl final {
		private:
			static_assert(
				std::is_same_v<T, ChildrenData> || std::is_same_v<T, const ChildrenData>,
				"ChildrenDataHolderImpl can only be instantiated with ChildrenData or const "
				"ChildrenData"
			);

			base::Ref<T> children;

			IF_BUILD_TYPE_DEV(mutable std::atomic_flag was_released = false;)

		public:
			ChildrenDataHolderImpl(base::Ref<T> children): children(children) {
				IF_BUILD_TYPE_DEV(children->lock->lock();)
			}

			ChildrenDataHolderImpl(const ChildrenDataHolderImpl&)            = delete;
			ChildrenDataHolderImpl& operator=(const ChildrenDataHolderImpl&) = delete;
			ChildrenDataHolderImpl(ChildrenDataHolderImpl&&)                 = delete;
			ChildrenDataHolderImpl& operator=(ChildrenDataHolderImpl&&)      = delete;

			auto operator*() -> std::conditional_t<
				std::is_const_v<T>,
				const std::vector<NodeID>&,
				std::vector<NodeID>&> {
				return children->children;
			}

			const std::vector<NodeID>& operator*() const { return children->children; }

			void release() const {
				IF_BUILD_TYPE_DEV({
					if (!was_released.test_and_set(std::memory_order_acquire))
						children->lock->unlock();
				})
			}

			~ChildrenDataHolderImpl() { this->release(); }
		};

		base::Box<concurrent::ConHashMap<NodeID, ChildrenData>> node_deps;

		/*
		 * for direct access to node_deps
		 */
		friend class QueryState;
		/**
		 * @brief Helper function to print nodes and their dependencies.
		 * @param nodes Vector of NodeIDs to print.
		 * @param out Output stream to print to.
		 */
		void debugPrintNodes(const std::vector<NodeID>& nodes, std::ostream& out) const;

	public:
		/**
		 * @brief Reduced graph representation used for compact serialization.
		 */
		struct ReducedGraphData final {
			std::vector<NodeID>             nodes;
			std::vector<std::vector<usize>> adjacency;
		};

		QueryGraph();
		QueryGraph(const QueryGraph&)            = delete;
		QueryGraph(QueryGraph&&)                 = default;
		QueryGraph& operator=(const QueryGraph&) = delete;
		QueryGraph& operator=(QueryGraph&&)      = delete;

		/**
		 * @brief Marks that given query depends on another query.
		 * Note that @p to does not need to be in the graph at the moment of calling this function.
		 */
		void addDependency(internal::NodeID from, internal::NodeID to);

		/**
		 * Returns all dependencies of a @p node_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDeps(internal::NodeID node_id) const;

		/**
		 * Returns all dependencies of a @p node_id of type @p dependency_id.
		 */
		[[nodiscard]]
		std::vector<NodeID> getNodeDepsFiltered(internal::NodeID node_id, QueryID dependency_id)
			const;

		/** @brief Returns the immediate dependencies of a @p node_id.
		 * @note This is not thread-safe and should only be used for debugging/testing purposes.
		 */
		[[nodiscard]] const std::vector<NodeID>& getDirectDependencies(const NodeID& node_id) const;

		void debugPrint(std::ostream& out) const;
		void debugPrintForDrawing(std::ostream& out) const;

		/**
		 * @brief Returns all dependencies of a given query call.
		 */
		template<class Query>
		auto getNodeDeps(typename Query::QKey key) const {
			internal::NodeID node_id = makeNodeID<Query>(key);
			return this->getNodeDeps(node_id);
		}

		/**
		 * @brief Returns all dependencies arising from @p dependency_id of a given query call.
		 */
		template<class Query>
		auto getNodeDepsFiltered(typename Query::QKey key, internal::QueryID dependency_id) const {
			internal::NodeID node_id = makeNodeID<Query>(key);
			return this->getNodeDepsFiltered(node_id, dependency_id);
		}

		/**
		 * @brief Serializes the QueryGraph into a vector of bytes.
		 * @note This DOES NOT optimize anything, it just serializes.
		 * @return A vector of bytes representing the serialized QueryGraph.
		 */
		[[nodiscard]] std::vector<byte> serialize() const;

		/**
		 * @brief Serializes an already reduced graph description.
		 * @details The provided mapping must mirror the exact structure we intend to persist, i.e.
		 * each adjacency index references the precomputed NodeID at the same position. This helper
		 * is meant for scenarios where another algorithm (e.g. QueryState::reduceOptimizeGraph) has
		 * already produced a compacted graph representation and we only need to emit bytes without
		 * rebuilding the mapping.
		 */
		[[nodiscard]] static std::vector<byte> serializeReducedGraph(ReducedGraphData reduced_graph);

		/**
		 * @brief Deserializes a QueryGraph from a vector of bytes.
		 * @param data The vector of bytes to deserialize from.
		 * @param node_mapper Optional mapper that can transform NodeIDs read from disk into the
		 *        NodeIDs that should be stored inside the graph. By default it is an identity
		 *        function, but callers can override it to keep the query framework state consistent.
		 * @return A deserialized QueryGraph object.
		 */
		static QueryGraph deserialize(
			std::span<const byte> data, std::function<NodeID(NodeID)> node_mapper = {}
		);

		/**
		 * @brief Compares this QueryGraph with another for equality. For testing purposes.
		 * @param other The other QueryGraph to compare with.
		 * @return True if the graphs are equal, false otherwise.
		 */
		[[nodiscard]] bool compare(const QueryGraph& other) const;

		/**
		 * @brief Checks if a node exists in the graph.
		 * @param node_id The NodeID to check.
		 * @return True if the node exists, false otherwise.
		 */
		[[nodiscard]]
		bool nodeExists(const NodeID& node_id) const {
			return node_deps->contains(node_id);
		}

		/**
		 * @brief Get all Nodes in the graph.
		 * @return A vector of all NodeIDs in the graph.
		 */
		[[nodiscard]] std::vector<NodeID> getAllNodes() const;

		/** @brief Check if a node has any dependencies. */
		[[nodiscard]] bool hasDependencies(const NodeID& node_id) const;

		~QueryGraph() = default;
	};
}
