#include "../../hierarchy/not_statements/expr_element.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	bool ExprElement::checkNonEmpty(LangParserState& state) {
		if (state.ctokens().size() == 0) {
			// Empty expression error
			state.logInt(makeBox<EmptyExprError>(state.getPosition()));
			return false;
		}
		return true;
	}
}
