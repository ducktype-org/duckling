#include "../../hierarchy/expressions/unit_expr.hpp"

#include "preamble.hpp"

#include <diagnostic_interactive/message.hpp>

namespace pst::expr {
	class BadUnitExprError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_unit_expr_error" };
		}

	public:
		BadUnitExprError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	MBox<ExprElement> UnitExpr::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		if (not(length == 1 && state[0].isBracketGroup(lexer::Token::Round))
		    || !state[0].getRecursive().empty()) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadUnitExprError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<UnitExpr>(state.getPosition());

		state.parse(out).goDown();
		state.parse(out).goUpAndSkip();

		return out;
	}

	void UnitExpr::dprint(std::ostream& out) const {
		out << "{";
		out << "}";
	}

	LangElement::HashAlg& UnitExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void UnitExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitUnitExpr(*this);
	}
}
