// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
		bool is(icu::UnicodeSet&) const;

		// The simple checks are inline, they are called for every character by the lexer.
		[[nodiscard]]
		bool is(UChar32 c) const {
			return c == value;
		}

		[[nodiscard]]
		bool isInRange(UChar32 begin, UChar32 end) const {
			return value >= begin && value <= end;
		}

		[[nodiscard]]
		bool isBinDigit() const {
			return is('0') or is('1');
		}

		[[nodiscard]]
		bool isOctDigit() const {
			return isInRange('0', '7');
		}

		[[nodiscard]]
		bool isDigit() const {
			return isInRange('0', '9');
		}

		[[nodiscard]]
		bool isHexDigit() const {
			return isInRange('0', '9') || isInRange('a', 'f') || isInRange('A', 'F');
		}

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
