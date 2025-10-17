#pragma once

#include "type_code.hpp"

#include <base/except/exceptions.hpp>
#include <base/types/ints.hpp>

#include <concepts>
#include <limits>

namespace hashing {


	namespace internal {

		/**
		 * Returns a unique ID on each call
		 */
		u64 getNextID();

		/**
		 * Returns a trully unique ID for the type
		 *
		 * @tparam T - type to get the unique ID for
		 * @tparam I - type of the value of the type code
		 */
		template<typename T, std::integral I>
		TypeCode<I, true> getUniqueID() {
			static const I id = static_cast<I>(getNextID());
			CORE_ASSERT(
				id < std::numeric_limits<I>::max(),
				"too many types have been created and the unique ID has wrapped around"
			);
			return TypeCode<I, true>{ id };
		}

	}  // namespace internal

	/**
	 * Returns a unique hash code of a given length for the type
	 *
	 * @tparam T - type to get the unique hash code for
	 * @tparam I - type of the value of the type code
	 */
	template<typename T, std::integral I = u32>
	static const TypeCode<I, true> TYPE_UNIQUE_CODE = internal::getUniqueID<T, I>();


}  // namespace hashing
