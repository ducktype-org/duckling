#pragma once

/**
 * @brief This function is used to invoke queries from "outside world".
 * It should never be used to invoke query from within query.
 */
template<typename QueryType>
auto queryEntryPoint(typename QueryType::QKey key) -> auto {
	// @TODO: proper way to do it
	// @TODO: who creates context?
	return QueryType::query(key);
}




