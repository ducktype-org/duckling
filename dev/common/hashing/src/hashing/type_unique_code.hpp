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

		u64 getNextID();

		/**
		 * returns a trully unique ID for the type
		 */
		template<typename T, std::integral I>
		TypeCodeBase<I> getUniqueID() {
			static const I id = static_cast<I>(getNextID());
			CORE_ASSERT(
				id < std::numeric_limits<I>::max(),
				"too many types have been created and the unique ID has wrapped around"
			);
			return TypeCodeBase<I>{ id };
		}

	}  // namespace detail

	/**
	 * returns a unique hash code of a given length for the type
	 */
	template<typename T, std::integral I = u32>
	static const TypeCodeBase<I> TYPE_UNIQUE_CODE = detail::getUniqueID<T, I>();


}  // namespace hashing
