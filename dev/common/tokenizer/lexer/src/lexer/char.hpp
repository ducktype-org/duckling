#pragma once

#include "classifications.hpp"
#include <base/raw_view.hpp>
#include <vector>
#include <string>
#include <optional>

namespace lexer {
	/**
	 * @brief Class representing a Unicode code point and allowing to check it's classifications
	 */
	class Char {
	public:

		const UChar32 value;
		const u8 size;
		const base::RawArray raw_begin;

		Char(UChar32, u8, base::RawArray);

		[[nodiscard]]
		bool is(icu::UnicodeSet&) const;
		[[nodiscard]]
		bool is(UChar32) const;
		[[nodiscard]]
		bool isInRange(UChar32 begin, UChar32 end) const;

		[[nodiscard]]
		bool isBinDigit() const;
		[[nodiscard]]
		bool isDigit() const;
		[[nodiscard]]
		bool isHexDigit() const;

		/**
		 * @brief Returns value of paired bracket for brackets or value of this character otherwise
		 */
		[[nodiscard]]
		UChar32 bracketPair() const;

		[[nodiscard]]
		std::string rawStr() const;
	};

	/**
	 * @brief Type used to store all characters decoded from a single source code
	 */
	using CharArray = const std::vector<Char>;

	/**
	 * @brief Assembles a `base::RawView` pointing to the placement of the given range in source code.
	 * 
	 * @param arr Array of characters in source code
	 * @param begin First character index in `arr` 
	 * @param end Last character index in `arr` 
	 */
	base::RawView composeRaw(CharArray& arr, usize begin, usize end);

}
