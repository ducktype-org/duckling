// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/box.hpp>

namespace tpc {
	class TokenStream;
}

namespace pst {
	class LangParserState;
	using tpc::TokenStream;
}

DEFAULT_BOX_PTR_DELETER_DECLARATION(pst::LangParserState)
