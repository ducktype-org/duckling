#include "automatic.hpp"

namespace tpc {
	NoIdentifierError::NoIdentifierError(dia::SourcePosition pos, std::string but_got):
		  MessageWithCodeFragmentAndCause(pos) {
		addArgument<dia_int::TextArgument>("but_got", std::move(but_got));
	}

	NoKeywordError::NoKeywordError(dia::SourcePosition pos, std::string but_got):
		  MessageWithCodeFragmentAndCause(pos) {
		addArgument<dia_int::TextArgument>("but_got", std::move(but_got));
	}

	NoOperatorError::NoOperatorError(dia::SourcePosition pos, std::string but_got):
		  MessageWithCodeFragmentAndCause(pos) {
		addArgument<dia_int::TextArgument>("but_got", std::move(but_got));
	}
}
