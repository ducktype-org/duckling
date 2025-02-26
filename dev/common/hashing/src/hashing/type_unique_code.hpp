#pragma once

#include <type_traits>
#include <concepts>
#include <cassert>
#include <limits>
#include <span>

#include <base/ints.hpp>

#include "type_code.hpp"

namespace hashing {


	namespace detail {

		// constexpr
		u64 getNextID() {
			static u64 id = 0;
			return id++;
		}

		template<typename T, std::integral I>
		TypeCodeBase<I> getUniqueID() {
			static const I id = static_cast<I>(getNextID());
			assert(id < std::numeric_limits<I>::max());
			return TypeCodeBase<I>{ id };
		}

	}  // namespace detail

	/**
	 * returns a unique hash code of a given length for the type
	 */
	template<typename T, std::integral I = u32>
	static const TypeCodeBase<I> TYPE_UNIQUE_CODE = detail::getUniqueID<T, I>();


}  // namespace hashing
