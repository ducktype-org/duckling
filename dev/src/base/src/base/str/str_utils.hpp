/**
 * @file str_utils.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 * @brief Provides a convenient set of utilities for concatenating string of various
 * representations into one, splitting strings, and replacing strings.
 *
 * Functionalities:
 * - `strConcat`
 * - `strSplit`
 * - `strReplaceAll`
 *
 * ### Usage
 * @include str_utils_example.cpp
 *
 * @example str_utils_example.cpp
 */
#pragma once

#include <base/comptime/type_traits.hpp>
#include <base/misc/raw_view.hpp>

#include <unicode/unistr.h>

#include <stdexcept>
#include <string>
#include <vector>

class Bits;
class Bytes;

namespace base {
	class StrID;

	// This is forward declaration to prevent circular header dependency
	template<typename EnumType>
	StrID enumToStr(EnumType v);

	namespace internal {

		template<typename T>
		concept HasEnumToStr = std::is_enum_v<T> && requires(T t) {
			{ base::enumToStr(t) } -> std::convertible_to<base::StrID>;
		};

		template<typename T>
		requires(!base::IsNumber<std::remove_reference_t<T>> && !std::is_same_v<icu::UnicodeString, std::remove_cvref_t<T>> && !std::is_pointer_v<std::decay_t<T>> && !std::is_null_pointer_v<std::decay_t<T>> && !HasEnumToStr<std::remove_cvref_t<T>>)
		void strConcat(std::string& out, T&& v) {
			out.append(std::forward<T>(v));
		}

		inline void strConcat(std::string& out, base::RawView view) {
			out.append(view.stringView());
		}

		inline void strConcat(std::string& out, base::IsNumber auto v) {
			out.append(std::to_string(v));
		}

		void strConcat(std::string& out, const icu::UnicodeString& unistr);

		// This is forward declaration to prevent circular header dependency through:
		// string_id.hpp -> maps.hpp -> exceptions.hpp -> str_utils.hpp
		void strConcat(std::string& out, base::StrID str_id);

		inline void strConcat(std::string& out, bool v) { out.append(v ? "true" : "false"); }

		inline void strConcat(std::string& out, const char* v) {
			if (v == nullptr) throw std::domain_error("strConcat called with nullptr");
			out.append(std::string(v));
		}

		template<typename U, typename V>
		requires(std::is_trivially_copyable_v<U> && std::is_trivially_copyable_v<V>)
		inline void strConcat(std::string& out, std::pair<U, V> pair) {
			out += "<";
			strConcat(out, pair.first);
			out += ", ";
			strConcat(out, pair.second);
			out += ">";
		}

		template<typename T>
		requires HasEnumToStr<std::remove_cvref_t<T>> void strConcat(std::string& out, T&& v) {
			// Use existing overload for StrID
			strConcat(out, base::enumToStr(std::forward<T>(v)));
		}

		inline void strConcat(std::string& out, Bits bits);
		inline void strConcat(std::string& out, Bytes bytes);
	}

	/**
	 * @brief Creates std::string from elements as if by executing the following pseudo code:
	 * ```
	 * std::string out;
	 * out.append(element_0)
	 * out.append(element_1)
	 * ...
	 * ```
	 *
	 * Specialization are provided for:
	 *
	 * - integer types - uses std::to_string,
	 * - RawView - uses RawView.str(), and
	 * - std::tuple - uses std::make_from_tuple<std::string>.
	 *
	 * Throws std::domain_error on nullptr argument.
	 */
	template<typename... T>
	std::string strConcat(T&&... elements) {
		std::string out;
		(internal::strConcat(out, std::forward<T>(elements)), ...);
		return out;
	}

	/**
	 * @brief Converts a single value to std::string using strConcat infrastructure.
	 *
	 * This function provides a user-extensible alternative to std::to_string that:
	 * - Can't break the std namespace
	 * - Supports all types that strConcat supports
	 * - Allows for custom user-defined string conversions
	 * - Is faster than std::to_string for concatenation scenarios
	 *
	 * @param value The value to convert to string
	 * @return String representation of the value
	 *
	 * @example
	 * ```cpp
	 * auto str1 = base::toString(42);           // "42"
	 * auto str2 = base::toString(MyEnum::Value); // "Value" (if stringifiable)
	 * auto str3 = base::toString(Bytes(1024));   // Custom conversion
	 * ```
	 */
	template<typename T>
	std::string toString(T&& value) {
		return strConcat("", std::forward<T>(value));
	}

	/**
	 * Replaces all occurrences of `from` with `to`.
	 * @param str The source string.
	 * @param from The pattern to be erased.
	 * @param to The pattern to be put instead of `from`.
	 */
	void strReplaceAll(std::string& str, const std::string& from, const std::string& to);

	/**
	 * Splits a string by a delimiter.
	 * @param str The string to split.
	 * @param delimiter A string, that is used to separate the substrings.
	 * @return A vector of separated strings.
	 */
	std::vector<std::string> strSplit(const std::string_view str, const std::string& delimiter = " ");
}
