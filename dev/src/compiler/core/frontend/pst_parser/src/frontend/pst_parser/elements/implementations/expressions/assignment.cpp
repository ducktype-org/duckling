#include "../../hierarchy/expressions/assignment.hpp"

#include "../../hierarchy/expressions/comma.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {

	MBox<ExprElement> Assignment::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		u64 length = state.ctokens().size();

		auto pos
			= dia::SourcePosition(state.getPosition(), state.getPosition((i64) length - 1).getEnd());

		bool found = false;
		i64  place = 0;
		for (i64 i = 0; i < length; i++) {
			if (ExprClassify::isAssignment(state.ctokens(), (i64) i)) {
				if (!found) {
					found = true;
					place = i;
				} else {
					state.logInt(makeBox<MultipleAssignmentError>(pos));
					return nullptr;
				}
			}
		}
		if (!found) return Lower::parse(state);
		auto out = makeBox<Assignment>(state);

		PARSE().autoFallbackLen(place).with(&out->variables, Lower::parse);

		PARSE().one(&out->type);

		PARSE().with(&out->value, Lower::parse);

		PST_RETURN out;
	}

	void Assignment::dprint(std::ostream& out) const {
		out << "{";

		out << R"("assigned variables": )";
		nullAwareDprint(variables, out);
		out << R"(, "assignment type": )";
		nullAwareDprint(type, out);
		out << R"(, "assigned value": )";
		nullAwareDprint(value, out);

		out << "}";
	}

	HashAlg& Assignment::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Assignment::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitAssignment(*this);
	}
}
