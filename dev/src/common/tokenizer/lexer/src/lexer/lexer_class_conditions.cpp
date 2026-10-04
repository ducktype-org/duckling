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
