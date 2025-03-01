#pragma once

#include <type_traits>
#include <concepts>
#include <cassert>
#include <limits>
#include <span>

#include <base/exceptions.hpp>
#include <base/ints.hpp>

#include "type_code.hpp"

namespace hashing {


	namespace detail {

		/**
		 * returns a unique ID on each call
		 */
		u64 getNextID();

		/**
		 * returns a trully unique ID for the type
		 *
		 * @tparam T - type to get the unique ID for
		 * @tparam I - type of the value of the type code
		 */
		template<typename T, std::integral I>
		TypeCodeBase<I, true> getUniqueID() {
			static const I id = static_cast<I>(getNextID());
			CORE_ASSERT(
				id < std::numeric_limits<I>::max(),
				"too many types have been created and the unique ID has wrapped around"
			);
			return TypeCodeBase<I, true>{ id };
		}

	}  // namespace detail

	/**
	 * returns a unique hash code of a given length for the type
	 *
	 * @tparam T - type to get the unique hash code for
	 * @tparam I - type of the value of the type code
	 */
	template<typename T, std::integral I = u32>
	static const TypeCodeBase<I, true> TYPE_UNIQUE_CODE = detail::getUniqueID<T, I>();


}  // namespace hashing
