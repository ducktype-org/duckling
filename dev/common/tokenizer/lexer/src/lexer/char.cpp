/**
 * @file char.cpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "char.hpp"
#include <base/exceptions.hpp>

namespace lexer {

	Char::Char(UChar32 value, u8 size, base::RawArray raw_begin):
		  value(value),
		  size(size),
		  raw_begin(raw_begin) {
		RIFT_ASSERT(raw_begin != nullptr, "Char constructed without raw pointer");
		if (size == u8{ 0 }) {
			RIFT_ASSERT(
				value == Classifications::end_of_file_value, "non-EOF Char created with size 0"
			);
		} else {
			RIFT_ASSERT(size > u8{ 0 } && size <= u8{ 4 }, "Char constructed with bad size");
		}
	}

	bool Char::is(icu::UnicodeSet& set) const { return set.contains(value); }

	bool Char::is(UChar32 c) const { return c == value; }

	bool Char::isInRange(UChar32 begin, UChar32 end) const {
		return value >= begin && value <= end;
	}

	bool Char::isBinDigit() const { return is('0') or is('1'); }

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

	base::RawView composeRaw(CharArray& array, usize from, usize to) {
		base::RawArray begin = array.at(from).raw_begin;
		usize          size  = array.at(to + 1).raw_begin - begin;
		return { begin, size };
	}
}
