#pragma once

#include <functional> // std::hash

namespace query {
	/**
	 * @brief Key used for queries without keys, input queries, and "outside world" query.
	 */
	struct EmptyKey {};
}

/**
 * @brief Hash implementation of EmptyKey.
 * It has to be here because makeNodeID is using it.
 */
template<>
struct std::hash<::query::EmptyKey> {
	std::size_t operator()([[maybe_unused]] const ::query::EmptyKey& key) const { return 0; }
};
