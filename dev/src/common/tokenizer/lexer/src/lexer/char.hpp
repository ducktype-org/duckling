#pragma once


#include <base/types/ints.hpp>

#include <unicode_classification/classifications.hpp>

#include <string>
#include <vector>

namespace lexer {

	/**
	 * @brief Class representing a Unicode code point and allowing to check it's classifications
	 */
	class Char final {
	public:
		const UChar32 value;
		const u8      size;
		const usize   index;

		Char(UChar32, u8, usize);

		[[nodiscard]]
		bool               is(icu::UnicodeSet&) const;
		[[nodiscard]] bool is(UChar32) const;
		[[nodiscard]]
		bool isInRange(UChar32 begin, UChar32 end) const;

		[[nodiscard]]
		bool isBinDigit() const;
		[[nodiscard]]
		bool isOctDigit() const;
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
	 * @brief Type used to store all characters decoded from a single source code file.
	 */
	using CharArray = std::vector<Char>;
}
