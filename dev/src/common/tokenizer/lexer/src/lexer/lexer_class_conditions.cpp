// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "lexer_class.hpp"

#include <unicode_classification/classifications.hpp>

namespace lexer {

	using Class = unicode::Classifications;

	bool Lexer::isEOF() const { return peek().is(Class::END_OF_FILE_VALUE); }

	bool Lexer::isEOL() const { return peek().is(Class::newline); }

	bool Lexer::isCommentBegin() const { return tryRawValue('#'); }

	usize Lexer::blockCommentOpeningLength() const {
		if (!isCommentBegin()) return 0;

		usize length = 1;
		if (peek(length).is(Class::name_start)) {
			do { ++length; } while (peek(length).is(Class::name_continue));
		}
		return tryRawValue('{', length) ? length + 1 : 0;
	}

	bool Lexer::isBlockCommentEnd(usize begin, usize delimiter_length) const {
		if (!tryRawValue('#') || !tryRawValue('}', delimiter_length - 1)) return false;
		for (usize offset = 1; offset + 1 < delimiter_length; ++offset)
			if (!peek(offset).is(char_array.at(begin + offset).value)) return false;
		return true;
	}

	bool Lexer::isStringBegin() const { return tryRawValue('"'); }

	bool Lexer::isFormatStringBegin() const { return tryRawValue('f') && tryRawValue('"', 1); }

	bool Lexer::isCharBegin() const { return tryRawValue('\''); }
}
