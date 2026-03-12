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
