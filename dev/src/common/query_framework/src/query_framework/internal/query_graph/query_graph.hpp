// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "node_id.hpp"
#include "node_making.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/base/locks/assert_lock.hpp>

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/config/build_type.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

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
			 * @note Holder uses assert lock, so if two threads try to get the holder at the same
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

			auto operator->() -> std::conditional_t<
				std::is_const_v<T>,
				const std::vector<NodeID>*,
				std::vector<NodeID>*> {
				return &children->children;
			}

			void release() const { IF_BUILD_TYPE_DEV({
				auto was_released_check = was_released.test_and_set(std::memory_order_acquire);
				CORE_ASSERT(!was_released_check, "ChildrenDataHolderImpl already released");

				children->lock->unlock();
			}) }

			/**
			 * This moves the hold children data out of the holder into a local object returned by
			 * this method. Importantly:
			 * - lock of the moved from object will be a nullptr after this, so any access to it
			 * will panic,
			 * - lock of the moved into object will be unlocked. New holder has to be created to
			 * access the children data after this move, as a reference hold in this lock will
			 * become dangling after the move.
			 */
			ChildrenData moveFrom() {
				// this sets the original lock to nullptr,
				// we have to do it first:
				auto moved_1 = std::move(*children);

				IF_BUILD_TYPE_DEV({
					bool was_released_check = was_released.test_and_set(std::memory_order_acquire);
					CORE_ASSERT(!was_released_check, "ChildrenDataHolderImpl already released");

					// we unlock not on *children, as that object is moved from, but on this local
					// one no one else can see (yet):
					moved_1.lock->unlock();
				})

				// we move again, to actually return the children data:
				return std::move(moved_1);
			}

			~ChildrenDataHolderImpl() {
				// we don't cal release() in the destructor, because we don't want to panic on
				// double release here.
				IF_BUILD_TYPE_DEV({
					auto was_released_check = was_released.test_and_set(std::memory_order_acquire);
					if (!was_released_check) children->lock->unlock();
				})
			}
		};

		base::Box<concurrent::ConHashMap<NodeID, ChildrenData>> node_deps;

		/**
		 * Graph that tracks the reversed relation to `node_deps`.
		 * Only used if @p getTrackReverseGraph() is true.
		 *
		 * Currently only used by the Language Server.
		 *
		 * Does not take part in any of the additional logic like serialization
		 * or deserialization.
		 */
		base::Box<concurrent::ConHashMap<NodeID, std::vector<NodeID>>> node_reverse_deps;

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
		 * Each adjacency entry holds indices into @c nodes
		 */
		struct ReducedGraphData final {
			std::vector<NodeID>             nodes;
			std::vector<std::vector<usize>> adjacency;

			/**
			 * @brief Whether the two vectors describe one graph: one adjacency list per node,
			 * and every index in range. This is used to check that a graph read from disk is
			 * consistent before rebuilding it.
			 * @return True if the graph is consistent, false otherwise.
			 * @note This does not check for cycles or other graph properties, only that the two
			 * vectors are consistent with each other.
			 */
			[[nodiscard]] bool isConsistent() const;
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

		/**
		 * @brief Returns the immediate dependencies of a @p node_id.
		 * @note This is not thread-safe and should only be used for debugging/testing purposes.
		 *       Access to the return reference can race with other operations.
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
		 * @brief The graph as the plain data that goes in the stream. This is used for testing only.
		 * @note This DOES NOT optimize anything, it just flattens: every node in the graph
		 * becomes an entry, and every dependency an index into it.
		 * In the production flow, the graph is optimized before being serialized, so this is not
		 * the form that goes in the stream.
		 * @return The nodes and their adjacency lists.
		 */
		[[nodiscard]] ReducedGraphData toReducedGraphData() const;

		/**
		 * @brief Rebuilds a graph from its serialized form.
		 * @details The mapping must mirror the exact structure that was persisted, i.e. each
		 * adjacency index references the NodeID at the same position.
		 * @param reduced_graph The nodes and their adjacency lists.
		 * @param node_mapper Optional mapper that can transform NodeIDs read from disk into the
		 *        NodeIDs that should be stored inside the graph. By default it is an identity
		 *        function, but callers can override it to keep the query framework state consistent.
		 *        In production the maping should never be indentity, but in testing it can be used.
		 * @return The graph those nodes describe, or nothing when @p reduced_graph is not
		 * one graph - see isConsistent().
		 */
		[[nodiscard]] static base::Optional<QueryGraph> fromReducedGraphData(
			ReducedGraphData reduced_graph, std::function<NodeID(NodeID)> node_mapper = {}
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
		 * @brief Helper to keep the output of the `getDependentNodes`
		 * function in a single struct, as it should be the transitive closure of
		 * the dependent nodes.
		 */
		struct Dependents {
			std::vector<NodeID> dependents_recursive;
		};

		/**
		 * @brief Gets the set of all nodes that are (transitively) dependent on any of the given
		 * start nodes, including the start nodes themselves.
		 *
		 * @warning This method should not be used when the query graph is being concurrently
		 * modified.
		 */
		[[nodiscard]] Dependents getDependentNodes(const std::vector<NodeID>& start_nodes) const;

		/**
		 * @brief Erase the given nodes from the graph. The nodes to erase should be obtained
		 * from getDependentNodes() to ensure all dependent nodes are erased.
		 *
		 * @warning This method should not be used when the query graph is being concurrently
		 * modified.
		 * @note This is for Language Server.
		 */
		void eraseNodes(const Dependents& nodes_to_erase);

		[[nodiscard]] std::vector<NodeID> getAllNodes() const;

		/** @brief Check if a node has any dependencies. */
		[[nodiscard]] bool hasDependencies(const NodeID& node_id) const;

		~QueryGraph() = default;
	};
}
