// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <hashing/hash.hpp>
#include <hashing/hashing_algorithms.hpp>

namespace pst {
	/**
	 * @brief Hash algorithm used for PST stable hashing
	 * @TODO: #1337 Swap to CRC256
	 */
	using HashAlg = hashing::StatefulHash<hashing::SHA256>;

	/**
	 * @brief Hash type for PST stable hashing
	 */
	using HashType = HashAlg::result_type;
}
