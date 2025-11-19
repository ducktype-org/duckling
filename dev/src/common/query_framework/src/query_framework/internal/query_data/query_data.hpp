#pragma once

#include <string_view>

namespace query::internal {

	/**
	 * General type of the query.
	 */
	enum class QueryType {
		Normal,
		SideInput,
		Input,
		Dummy  //> Query with that type should never be called or implemented. This is used in
		       // incremental compilation when inserting dummy nodes to current graph from previous
		       // graph.
	};

	/**
	 * @brief Query tags are various, lightweight attributes or properties that can be associated
	 * with a query. Query tags provide additional metadata about the query's behavior,
	 * characteristics, or requirements.
	 *
	 * @note Some of the tags are only relevant to the query implementation,
	 * but all the tags are kept in QueryData for simplicity.
	 *
	 * @note We use aggrate initialization for QueryTags, we can emulate default value + named
	 * arguments, since most queries will only set a few tags different than default (or none).
	 *
	 * @TODO PR: move it out of internall?
	 */
	struct QueryTags final {
		// Helper type definitions:
		enum class UsedHashes {
			UnstableHash,
			StableHash,
		};


		// Tags:

		/**
		 * Type of hash used by the query.
		 */
		UsedHashes used_hashes = UsedHashes::UnstableHash;

		/**
		 * Whether query is cached on disk.
		 * Queries cached on disk must use stable hashing and provide loadFromDisc function.
		 */
		bool is_cached_on_disk = false;

		// additional methods:

		[[nodiscard]]
		bool isHashStable() const {
			return used_hashes == UsedHashes::StableHash;
		}

		/**
		 * Verify that the tags are consistent.
		 * For example, if is_cached_on_disk is true, then used_hashes must be StableHash.
		 * Is run in runtime by the query framework when registering the query.
		 * @TODO PR: do it
		 */
		void verify() const;
	};

	/**
	 * Struct holding universal, comp-time meta data of each query type.
	 * It is set per query, and stored in the query-interface struct, so it can be accessed anywhere
	 * in the pogram. Additionally it is stored in QueryID data, so it can be accessed from QueryID
	 * as well, without knowning the comp-time type of the query.
	 */
	struct QueryData final {
		QueryType        type;
		std::string_view name;
		QueryTags        tags;

		constexpr QueryData(QueryType type, std::string_view name, QueryTags tags):
			  type(type),
			  name(name),
			  tags(tags) {}

		constexpr QueryData(const QueryData&) = default;
	};
};
