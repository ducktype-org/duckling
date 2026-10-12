// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file char.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "char.hpp"

#include <base/except/exceptions.hpp>

#include <unicode_classification/classifications.hpp>

namespace lexer {

	Char::Char(UChar32 value, u8 size, usize index): value(value), size(size), index(index) {
		if (size == u8{ 0 }) {
			CORE_ASSERT(
				value == unicode::Classifications::END_OF_FILE_VALUE,
				"non-EOF Char created with size 0"
			);
		} else {
			CORE_ASSERT(size > u8{ 0 } && size <= u8{ 4 }, "Char constructed with bad size");
		}
	}

	bool Char::is(icu::UnicodeSet& set) const { return set.contains(value); }

	bool Char::is(UChar32 c) const { return c == value; }

	bool Char::isInRange(UChar32 begin, UChar32 end) const {
		return value >= begin && value <= end;
	}

	bool Char::isBinDigit() const { return is('0') or is('1'); }

	bool Char::isOctDigit() const { return isInRange('0', '7'); }

	bool Char::isDigit() const { return isInRange('0', '9'); }

	bool Char::isHexDigit() const {
		return isInRange('0', '9') || isInRange('a', 'f') || isInRange('A', 'F');
	}

	UChar32 Char::bracketPair() const { return u_getBidiPairedBracket(value); }

	std::string Char::rawStr() const {
		std::string res;
		icu::UnicodeString(value).toUTF8String(res);
		return res;
	}
}
