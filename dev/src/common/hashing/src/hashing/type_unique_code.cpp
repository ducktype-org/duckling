#include "type_unique_code.hpp"

#include <base/types/ints.hpp>

namespace hashing::internal {


	/**
	 * Returns a unique ID on each call
	 */
	u64 getNextID() {
		static u64 id = 0;
		return id++;
	}


}  // namespace hashing::internal
