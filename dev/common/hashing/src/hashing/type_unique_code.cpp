#include "type_unique_code.hpp"

#include <base/ints.hpp>

namespace hashing::detail {


	/**
	 * Returns a unique ID on each call
	 */
	u64 getNextID() {
		static u64 id = 0;
		return id++;
	}


}  // namespace hashing::detail
