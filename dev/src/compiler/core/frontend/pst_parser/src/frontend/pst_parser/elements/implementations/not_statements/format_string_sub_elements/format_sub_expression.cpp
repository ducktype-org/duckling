
#include "../../../hierarchy/not_statements/format_string_sub_elements/format_sub_expression.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(FormatSubExpression, expr);

	MBox<FormatSubExpression> FormatSubExpression::parse(LangParserState& state) {
		auto out = makeBox<FormatSubExpression>(state);

		PARSE().goDown();
		PARSE().one(&out->expr);
		PARSE().goUpAndSkip();

		PST_RETURN out;
	}

	void FormatSubExpression::dprint(std::ostream& out) const { nullAwareDprint(expr, out); }

	HashAlg& FormatSubExpression::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}
}
