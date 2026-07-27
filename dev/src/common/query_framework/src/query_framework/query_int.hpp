/**
 * @file query_int.hpp
 * @brief Implementation of automatic generation of query interfaces.
 */
#pragma once

// clang-format off
// clang format from GH action gets confused here for some reason, see:
// https://github.com/ducktype-org/duckling/pull/657#pullrequestreview-2732904586
// https://github.com/ducktype-org/duckling/pull/657#pullrequestreview-2732904586

#include <query_framework/context/context_fd.hpp>            // IWYU pragma: export
#include <query_framework/internal/query_graph/node_id.hpp>  // IWYU pragma: export
#include <query_framework/internal/query_data/query_id.hpp>  // IWYU pragma: export
#include <query_framework/utils/simple_keys.hpp>             // IWYU pragma: export
#include <query_framework/utils/query_hash.hpp>              // IWYU pragma: export

#include <base/preproc/macro_base.hpp>

#include <string_view>  // IWYU pragma: export

// clang-format on

namespace query::internal {
	// Forward declaration for friendship
	struct EntryPointHelper;
}

/**
 * @brief Internal macro used do delcare queries.
 * Should not be used directly.
 *
 * @note QUERY_INTERFACE_TAG is just a dummy member to tag query interfaces.
 *
 * @param query_type Name of the query
 * @param key_mp Type of the query key
 * @param result_mp Type of the query result
 * @param query_data_mp Query data struct
 */
#define DECLARE_QUERY_AUX(query_type, key_mp, result_mp, query_data_mp)                            \
	struct query_type final {                                                                      \
		using QueryType = query_type;                                                              \
		using QKey      = key_mp;                                                                  \
		using QResult   = result_mp;                                                               \
                                                                                                   \
	private:                                                                                       \
		static auto                       internal_query(const QKey&) -> QResult;                  \
		static auto                       internal_load(::query::QueryStableHash hash) -> QResult; \
		static auto                       internal_erase(::query::QueryStableHash) -> bool;        \
		static auto                       internal_disk_erase(::query::QueryStableHash) -> bool;   \
		static ::query::internal::QueryID id;                                                      \
		friend struct ::query::Context;                                                            \
		friend struct ::query::internal::EntryPointHelper;                                         \
                                                                                                   \
	public:                                                                                        \
		static constexpr ::query::internal::QueryData QUERY_DATA          = query_data_mp;         \
		static constexpr bool                         QUERY_INTERFACE_TAG = true;                  \
		static auto                                   getID() { return id; }                       \
	};

/**
 * @brief Macro used do delcare queries.
 *
 * For example:
 * `DECLARE_QUERY (QueryName, QueryKey, QueryReturnValue, ({ / * non-default tags * / }))`
 *
 * @note Tags must be passed as parenthesized list, e.g. `({})` or `({Tag1, Tag2})`.
 * This way there are no issues with commas in macro arguments.
 * Lack of parentheses should produce compilation errors.
 */
#define DECLARE_QUERY(query_type, key, value, tags)                     \
	DECLARE_QUERY_AUX(                                                  \
		query_type,                                                     \
		key,                                                            \
		value,                                                          \
		::query::internal::QueryData(                                   \
			::query::internal::QueryKind::Normal,                       \
			#query_type,                                                \
			::query::internal::QueryTags EXPAND tags,                   \
			{                                                           \
				.erase_function      = query_type::internal_erase,      \
				.disk_erase_function = query_type::internal_disk_erase, \
			}                                                           \
		)                                                               \
	)
