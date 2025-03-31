/**
 * @file anycast.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 *
 * ### Usage:
 * @include anycast_example.cpp
 *
 * @example anycast_example.cpp
 */

#pragma once

#include <any>

#include "exceptions.hpp"

namespace base {
	/**
	 * This is a helper function for std::any_cast - it tries to perform a cast and on error prints
	 * more information. Instead of: `std::bad_any_cast: bad any cast` it prints - depending on the
	 * compiler - type of std::any and type of T.
	 * @return `value` cast to type `T`
	 */
	template<class T>
	constexpr T anyCast(const std::any& value) {
		try {
			return std::any_cast<T>(value);
		} catch (std::bad_any_cast&) {
			throw base::LogicError(base::strConcat(
				"Bad any_cast: Value is of ",
#ifdef _MSC_VER
				"type : \"",
				value.type().name(),  // This function works very poorly in gcc/clang - displays
			                          // only the first letter of the type - for i64 == 'l'
				"\" instead of \"",
#else
				"different type than \"",
#endif
				base::typeName<T>(),
				"\""
			));
		}
	}
}
