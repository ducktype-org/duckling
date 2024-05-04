#pragma once

#include "query_id_provider.hpp"
#include "node_making.hpp"
#include "query_int.hpp"
#include "empty_key.hpp"

namespace query {

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 */
	template<typename QueryType>
	auto entryPoint(typename QueryType::QKey key) -> decltype(auto) {
		return QueryType::internal_query(
			key, makeNodeID(detail::outsideWorldQueryID(), EmptyKey())
		);
	}
}
