#pragma once

#include <base/except/exceptions.hpp>
#include <base/types/ok_bad.hpp>

#include <query_framework/utils/query_hash.hpp>

#include <string_view>

namespace query {

	/**
	 * Hash used by the query.
	 */
	enum class UsedHashes {
		UnstableHash,
		StableHash,
	};

	namespace internal {

		/**
		 * General type of the query.
		 */
		enum class QueryKind {
			Normal,
			SideInput,
			Input,
			Dummy  //> Query with that type should never be called or implemented. This is used in
			       // incremental compilation when inserting dummy nodes to current graph from
			       // previous graph.
		};

		/**
		 * @brief Query tags are various, lightweight attributes or properties that can be
		 * associated with a query, other then its primary type and a name. Query tags provide
		 * additional metadata about the query's behavior, characteristics, or requirements.
		 *
		 * @note Some of the tags are only relevant to the query implementation,
		 * but all the tags are kept in QueryData for simplicity.
		 *
		 * @note We use aggrate initialization for QueryTags, we can emulate default value + named
		 * arguments, since most queries will only set a few tags different than default (or none).
		 */
		struct QueryTags final {
			/**
			 * Type of hash used by the query.
			 */
			UsedHashes used_hashes = UsedHashes::UnstableHash;

			/**
			 * Whether query result is cached on disk and can be loaded from there in incremental
			 * compilation. Queries cached on disk must use stable hashing and provide loadFromDisk
			 * function.
			 */
			bool can_be_loaded_from_disk = false;

			/**
			 * Whether the query node should be preserved in the graph and on disk after computation.
			 * This does not imply that the query result can be loaded from disk.
			 * Nodes with this tag will not be removed during incremental graph optimizations.
			 */
			bool preserve_in_graph = false;

			/**
			 * Whether query uses the failable QResult type as the result type.
			 */
			bool uses_qresult = true;

			/**
			 * Whether the query implementation should catch  QueryFailedException exception
			 * thrown from provide() function, and convert it into QResult::Failed() value.
			 * Relevant only if `uses_qresult` is true, otherwise the tag is ignored.
			 */
			bool catch_exceptions_if_using_qresult = true;

			/**
			 * Whether the query is allowed to be cyclic.
			 * Relevant only if `uses_qresult` is true, otherwise the tag is ignored.
			 */
			bool allow_cycles = true;
		};

		/**
		 * @brief Default erase hook that panics when invoked.
		 */
		[[noreturn]]
		inline bool panicUnwiredErase(QueryStableHash) {
			CORE_PANIC("Query erase function was not wired for this query; this is a bug.");
		}

		/**
		 * @brief Struct holding all the data related to query caching, like erase function pointer.
		 */
		struct QueryCacheData final {
			using InternalEraseFunctionType = bool (*)(QueryStableHash);

			/**
			 * Pointer to the function that can erase the query result from its in-memory cache
			 * based on the key hash. Defaults to a panic so an unset hook is never a silent no-op.
			 */
			InternalEraseFunctionType erase_function = panicUnwiredErase;

			/**
			 * Pointer to the function that can erase the query result from its on-disk cache based
			 * on the key hash. Defaults to a panic so an unset hook is never a silent no-op.
			 */
			InternalEraseFunctionType disk_erase_function = panicUnwiredErase;
		};

		/**
		 * Struct holding universal, comp-time meta data of each query type.
		 * It is set per query, and stored in the query-interface struct, so it can be accessed
		 * anywhere in the pogram. Additionally it is stored in QueryID data, so it can be accessed
		 * from QueryID as well, without knowning the comp-time type of the query.
		 *
		 * Query data consist of three main parts:
		 * - type of the query (e.g. normal, input, side-input, dummy, see: QueryKind)
		 * - name of the query
		 * - various tags associated with the query (see QueryTags)
		 *
		 * @note QueryData and QueryTags struct are internal, since they should probably not be
		 * named directly outside the query framework. Its however valid, to use it, when there is
		 * some indirect access to it, e.g. via QueryID or query interface struct.
		 */
		struct QueryData final {
			QueryKind        kind;
			std::string_view name;
			QueryTags        tags;
			QueryCacheData   cache_data;

			constexpr QueryData(
				QueryKind kind, std::string_view name, QueryTags tags, QueryCacheData cache_data
			):
				  kind(kind),
				  name(name),
				  tags(tags),
				  cache_data(cache_data) {}

			constexpr QueryData(const QueryData&) = default;

			[[nodiscard]]
			constexpr bool isInputQuery() const {
				return kind == QueryKind::Input or kind == QueryKind::SideInput;
			}

			[[nodiscard]]
			constexpr bool usesStableHashing() const {
				return tags.used_hashes == UsedHashes::StableHash;
			}

			[[nodiscard]]
			constexpr bool usesUnstableHashing() const {
				return tags.used_hashes == UsedHashes::UnstableHash;
			}

			/**
			 * Verify that the query data is consistent, including the tag data.
			 * For example, if can_be_loaded_from_disk is true, then used_hashes must be StableHash.
			 * Is run in comptime time in query implementation boilerplate.
			 */
			[[nodiscard]]
			constexpr base::OkBad verify() const {
				if (tags.can_be_loaded_from_disk) {
					// queries that are cached on disk must use stable hashing:
					if (tags.used_hashes != UsedHashes::StableHash) return base::BAD;
					// queries that can be loaded from disk must have preserve_in_graph true:
					if (!tags.preserve_in_graph) return base::BAD;
				}
				if (isInputQuery()) {
					// input queries must use stable hashing:
					if (tags.used_hashes != UsedHashes::StableHash) return base::BAD;
					// inputs must be preserved on disk:
					if (!tags.preserve_in_graph) return base::BAD;
				}

				return base::OK;
			}
		};
	}
};
