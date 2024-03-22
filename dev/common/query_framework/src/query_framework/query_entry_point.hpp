#pragma once

#include "dep_graph.hpp"

namespace query {

	/**
	 * @brief This function is used to invoke queries from "outside world".
	 * It should never be used to invoke query from within query.
	 */
	template<typename QueryType>
	auto queryEntryPoint(typename QueryType::QKey key) -> auto {
		return QueryType::internal_query(key, makeNodeID(outsideWorldQueryID(), EmptyKey()));
	}

}
