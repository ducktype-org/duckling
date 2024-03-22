#pragma once

#include "query_id_provider.hpp"
#include "node_making.hpp"

namespace query {

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 */
	template<typename QueryType>
	auto queryEntryPoint(typename QueryType::QKey key) -> auto {
		return QueryType::internal_query(key, detail::makeNodeID(detail::outsideWorldQueryID(), EmptyKey()));
	}
}
