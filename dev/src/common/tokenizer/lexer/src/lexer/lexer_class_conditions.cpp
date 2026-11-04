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

	bool Lexer::isBlockCommentBegin() const { return tryRawValue('#') && tryRawValue('{', 1); }

	bool Lexer::isBlockCommentEnd() const { return tryRawValue('#') && tryRawValue('}', 1); }

	bool Lexer::isStringBegin() const { return tryRawValue('"'); }

	bool Lexer::isFormatStringBegin() const { return tryRawValue('f') && tryRawValue('"', 1); }

	bool Lexer::isCharBegin() const { return tryRawValue('\''); }
}
