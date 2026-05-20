#include "../../hierarchy/expressions/cast_as.hpp"

#include "../../hierarchy/expressions/logic_or.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> CastAs::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		bool as_found = false;
		i64  as_fwd   = 0;

		// Left-associative: bind the rightmost `as` at this precedence level first.
		for (i64 i = length - 1; i >= 0; i--) {
			if (state[i].is(Keyword::As)) {
				as_found = true;
				as_fwd   = i;
				break;
			}
		}
		if (!as_found) return Lower::parse(state);

		auto out = makeBox<CastAs>(state);

		PARSE().autoFallbackLen(as_fwd).with(&out->value, Self::parse);
		PARSE().one(Keyword::As);
		PARSE().with(&out->type, Lower::parse);

		PST_RETURN out;
	}

	void CastAs::dprint(std::ostream& out) const {
		out << "{";

		out << R"("value": )";
		nullAwareDprint(value, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);

		out << "}";
	}

	HashAlg& CastAs::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void CastAs::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitCastAs(*this); }
}
