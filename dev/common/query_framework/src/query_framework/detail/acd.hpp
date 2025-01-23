/**
 * @file acd.hpp
 * @brief Implementation of :code:`ACD` -- a simple structure that defines
 * additional data that needs to be stored along side every cache entry.
 */
#pragma once

namespace query {

	/**
	 * @brief Additional Cache data.
	 * This is a struct that needs to be stored along side every "cache entry".
	 */
	struct ACD final {
		// ...
	};

	template<typename Data>
	struct CacheEntry final {
		Data data;
		ACD  acd;
	};
}
