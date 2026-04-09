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
