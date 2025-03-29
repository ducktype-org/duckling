/**
 * @file query_int.hpp
 * @brief Implementation of automatic generation of query interfaces.
 */
#pragma once

#include <string_view>          // IWYU pragma: export
#include "detail/query_id.hpp"  // IWYU pragma: export
#include "detail/node_id.hpp"   // IWYU pragma: export
#include "empty_key.hpp"        // IWYU pragma: export
#include "context_fd.hpp"        // IWYU pragma: export

namespace query::detail {

	struct ContextType;
	struct EntryPointHelper;

	/**
	 * @brief Base class for defining query interface
	 *
	 * @tparam QueryType_tp a type of a query
	 * @tparam QKey_tp a type of a query key
	 * @tparam QResult_tp a type of a query result
	 */
	template<typename QueryType_tp, typename QKey_tp, typename QResult_tp>
	struct QueryInterface {
		using QueryType = QueryType_tp;
		using QKey      = QKey_tp;
		using QResult   = QResult_tp;
	};
}

/**
 * @brief Macro used do delcare queries.
 *
 * For example:
 * `DECLARE_QUERY (QueryName, QueryKey, QueryReturnValue)`
 */
#define DECLARE_QUERY(query_type, key, value)                                                     \
	struct query_type: ::query::detail::QueryInterface<query_type, key, value> {                  \
	private:                                                                                      \
		static auto                     internal_query(QKey, ::query::detail::NodeID) -> QResult; \
		static ::std::string_view       name;                                                     \
		static ::query::detail::QueryID id;                                                       \
		friend struct ::query::detail::ContextType;                                               \
		friend struct ::query::detail::EntryPointHelper;                                          \
                                                                                                  \
	public:                                                                                       \
		static auto getName() { return name; }                                                    \
		static auto getID() { return id; }                                                        \
	};
