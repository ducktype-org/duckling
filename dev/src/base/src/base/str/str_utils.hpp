// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
 * - `unescapeString`
 * - `escapeString`
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

#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


class Bits;
class Bytes;

namespace base {
	class StrID;

	// This is forward declaration to prevent circular header dependency
	template<typename EnumType>
	std::string_view enumToStr(EnumType v);

	namespace internal {

		template<typename T>
		concept HasEnumToStr = std::is_enum_v<T> && requires(T t) {
			{ base::enumToStr(t) } -> std::convertible_to<std::string_view>;
		};

		template<typename T>
		requires(!base::IsNumber<std::remove_reference_t<T>> && !std::is_same_v<icu::UnicodeString, std::remove_cvref_t<T>> && !std::is_pointer_v<std::decay_t<T>> && !std::is_null_pointer_v<std::decay_t<T>> && !HasEnumToStr<std::remove_cvref_t<T>>)
		constexpr void strConcat(std::string& out, T&& v) {
			out.append(std::forward<T>(v));
		}

		constexpr void strConcat(std::string& out, base::RawView view) {
			out.append(view.stringView());
		}

		template<base::IsNumber T>
		constexpr void strConcat(std::string& out, T v) {
			out.append(std::to_string(v));
		}

		void strConcat(std::string& out, const icu::UnicodeString& unistr);

		// This is forward declaration to prevent circular header dependency through:
		// string_id.hpp -> maps.hpp -> exceptions.hpp -> str_utils.hpp
		void strConcat(std::string& out, base::StrID str_id);

		constexpr void strConcat(std::string& out, char v) { out.append(1uz, v); }

		constexpr void strConcat(std::string& out, bool v) { out.append(v ? "true" : "false"); }

		constexpr void strConcat(std::string& out, const char* v) {
			if (v == nullptr) throw std::domain_error("strConcat called with nullptr");
			out.append(v);
		}

		template<typename U, typename V>
		requires(std::is_trivially_copyable_v<U> && std::is_trivially_copyable_v<V>)
		constexpr void strConcat(std::string& out, std::pair<U, V> pair) {
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

		constexpr void strConcat(std::string& out, Bits bits);
		constexpr void strConcat(std::string& out, Bytes bytes);

		constexpr void strConcat(std::string& out, std::monostate) {
			strConcat(out, "<monostate>");
		}
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
	constexpr std::string strConcat(T&&... elements) {
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
	constexpr std::string toString(T&& value) {
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

	/**
	 * @brief Joins a range of strings into a single string with a specified separator.
	 */
	std::string strJoin(std::ranges::input_range auto&& range, std::string_view sep) {
		std::string result;

		auto it  = std::ranges::begin(range);
		auto end = std::ranges::end(range);

		if (it == end) return result;

		result += *it;
		++it;

		for (; it != end; ++it) {
			result += sep;
			result += *it;
		}

		return result;
	}

	/**
	 * @brief Trims leading whitespace from a string view.
	 *
	 * @param text Input string view.
	 * @param ws_chars Characters treated as whitespace.
	 * @return String view without leading whitespace.
	 */
	[[nodiscard]] constexpr std::string_view strTrimLeft(
		std::string_view text, std::string_view ws_chars = " \t\r\n\f\v"
	) {
		auto first_not_ws = text.find_first_not_of(ws_chars);
		if (first_not_ws == std::string_view::npos) return {};
		return text.substr(first_not_ws);
	}

	/**
	 * @brief Trims leading and trailing whitespace from a string view.
	 *
	 * @param text Input string view.
	 * @param ws_chars Characters treated as whitespace.
	 * @return String view without leading/trailing whitespace.
	 */
	[[nodiscard]] constexpr std::string_view strTrim(
		std::string_view text, std::string_view ws_chars = " \t\r\n\f\v"
	) {
		auto first_not_ws = text.find_first_not_of(ws_chars);
		if (first_not_ws == std::string_view::npos) return {};

		auto last_not_ws = text.find_last_not_of(ws_chars);
		return text.substr(first_not_ws, last_not_ws - first_not_ws + 1);
	}

	/**
	 * @brief Generates a random alphanumeric string of the specified length.
	 * @param length The length of the random string to generate.
	 * @return A random alphanumeric string.
	 */
	std::string generateRandomString(u64 length);

	struct UnescapedString {
		// The successfully unescaped string.
		std::string value;
	};

	struct UnknownEscapeSequence {
		// The unknown escape sequence that caused the error.
		std::string value;
	};

	using UnescapeResult = std::expected<UnescapedString, UnknownEscapeSequence>;

	/**
	 * @brief Unescapes a string containing C-style escape sequences.
	 * @param raw The raw string with escape sequences.
	 * @return The unescaped string.
	 *
	 * Supported escape sequences:
	 * - \n : Newline
	 * - \r : Carriage return
	 * - \t : Tab
	 * - \v : Vertical tab
	 * - \b : Backspace
	 * - \f : Form feed
	 * - \a : Alert (bell)
	 * - \e : Escape (non-standard but common)
	 * - \\ : Literal backslash
	 * - \" : Double quote
	 * - \' : Single quote
	 * - \0 : Null character
	 *
	 * Unknown escape sequences result in an error, and the unescaping process is aborted. The error
	 * contains the unknown escape sequence.
	 */
	UnescapeResult unescapeString(std::string_view raw);

	/**
	 * @brief Escapes special characters in a string using C-style escape sequences.
	 * @param raw The raw string to escape.
	 * @return The escaped string.
	 */
	std::string escapeString(std::string_view raw);
}
