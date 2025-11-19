/**
 * @file query_int.hpp
 * @brief Implementation of automatic generation of query interfaces.
 */
#pragma once

// clang-format off
// clang format from GH action gets confused here for some reason, see:
// https://github.com/ducktype-org/duckling/pull/657#pullrequestreview-2732904586
// https://github.com/ducktype-org/duckling/pull/657#pullrequestreview-2732904586

#include "context_fd.hpp"                    // IWYU pragma: export
#include "internal/query_graph/node_id.hpp"  // IWYU pragma: export
#include "internal/query_data/query_id.hpp"  // IWYU pragma: export
#include "empty_key.hpp"                     // IWYU pragma: export

#include <string_view>  // IWYU pragma: export
#include <base/preproc/remove_parentheses.hpp>

// clang-format on

namespace query::internal {
	// Forward declaration for friendship
	struct EntryPointHelper;
}

/**
 * @brief Internal macro used do delcare queries.
 * Should not be used directly.
 *
 * @param query_type Name of the query
 * @param key_mp Type of the query key
 * @param result_mp Type of the query result
 * @param query_data_mp Query data struct
 */
#define DECLARE_QUERY_AUX(query_type, key_mp, result_mp, query_data_mp)                \
	struct query_type final {                                                          \
		using QueryType = query_type;                                                  \
		using QKey      = key_mp;                                                      \
		using QResult   = result_mp;                                                   \
                                                                                       \
	private:                                                                           \
		static auto internal_query(const QKey&, ::query::internal::NodeID) -> QResult; \
		static ::query::internal::QueryID             id;                              \
		friend struct ::query::Context;                                                \
		friend struct ::query::internal::EntryPointHelper;                             \
                                                                                       \
	public:                                                                            \
		static constexpr ::query::internal::QueryData QUERY_DATA = query_data_mp;      \
		static auto            getID() { return id; }                                  \
		static constexpr auto& getData() { return QUERY_DATA; }                        \
	};
	// @TODO PR: remove get_data accessor, its constexpr anyway

/**
 * @brief Macro used do delcare queries.
 *
 * For example:
 * `DECLARE_QUERY (QueryName, QueryKey, QueryReturnValue, ({ / * non-default tags * / }))`
 */
#define DECLARE_QUERY(query_type, key, value, tags)                                           \
	DECLARE_QUERY_AUX(                                                                        \
		query_type,                                                                           \
		key,                                                                                  \
		value,                                                                                \
		::query::internal::QueryData( \
			::query::internal::QueryType::Normal, \
			#query_type, \
			::query::internal::QueryTags REMOVE_PARENTHESES(tags)\
		)\
			\
	)
