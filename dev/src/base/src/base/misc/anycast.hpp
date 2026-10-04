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

#include <base/except/exceptions.hpp>

#include <any>

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
		} catch (std::bad_any_cast& err) {
			throw base::LogicError(base::strConcat(
				"Bad any_cast: Value is of ",
				"type : \"",
				value.type().name(),
				"\" instead of \"",
				base::typeName<T>(),
				"\" \n STD exception message: ",
				err.what()
			));
		}
	}
}
