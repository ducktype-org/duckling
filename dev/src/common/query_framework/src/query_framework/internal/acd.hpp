// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
