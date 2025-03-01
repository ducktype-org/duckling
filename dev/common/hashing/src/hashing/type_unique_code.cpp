#include <type_traits>
#include <concepts>
#include <cassert>
#include <limits>
#include <span>

#include <base/ints.hpp>

#include "type_code.hpp"

namespace hashing::detail {


	/**
	 * returns a unique ID on each call
	 */
	u64 getNextID() {
		static u64 id = 0;
		return id++;
	}


}  // namespace hashing::detail
