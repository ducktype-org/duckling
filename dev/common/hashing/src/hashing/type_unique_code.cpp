#include <type_traits>
#include <concepts>
#include <cassert>
#include <limits>
#include <span>

#include <base/ints.hpp>

#include "type_code.hpp"
#include "type_code.hpp"

namespace hashing {

	namespace detail {

		u64 getNextID() {
			static u64 id = 0;
			return id++;
		}

	}  // namespace detail

}  // namespace hashing
