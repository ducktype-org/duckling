// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "automatic.hpp"

namespace tpc {
	NoIdentifierError::NoIdentifierError(dia::SourcePosition pos, std::string_view but_got):
		  MessageWithCodeFragmentAndCause(pos) {
		addArgument<dia::TextArgument>("but_got", std::string(but_got));
	}

	NoKeywordError::NoKeywordError(dia::SourcePosition pos, std::string_view but_got):
		  MessageWithCodeFragmentAndCause(pos) {
		addArgument<dia::TextArgument>("but_got", std::string(but_got));
	}

	NoOperatorError::NoOperatorError(dia::SourcePosition pos, std::string_view but_got):
		  MessageWithCodeFragmentAndCause(pos) {
		addArgument<dia::TextArgument>("but_got", std::string(but_got));
	}
}
