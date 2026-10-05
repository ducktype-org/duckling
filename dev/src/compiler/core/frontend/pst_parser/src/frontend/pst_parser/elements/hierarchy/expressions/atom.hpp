// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief This is a helper element for parsing literals and bracket subexpressions that
	 * decides which literal to parse.
	 */
	class Atom: public ExprElement {
	public:
		Atom() = delete;

		static MBox<ExprElement> parse(LangParserState& state);
	};
}
