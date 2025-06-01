#include "../../hierarchy/expressions/assignment.hpp"

#include "../../hierarchy/expressions/comma.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	class MultipleAssignmentError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Multiple assignments in one expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MultipleAssignmentError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> Assignment::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos
			= dia::SourcePosition(state.getPosition(), state.getPosition((i64) length - 1).getEnd());

		bool found = false;
		i64  place = 0;
		for (i64 i = 0; i < length; i++) {
			if (ExprClassify::isAssignment(state, (i64) i)) {
				if (!found) {
					found = true;
					place = i;
				} else {
					state.log(makeBox<MultipleAssignmentError>(pos));
					fastForward(state, length);
					return nullptr;
				}
			}
		}
		if (!found) return Lower::parse(state, length);
		auto out = makeBox<Assignment>(pos);

		state.parse(out).with(&out->variables, Lower::parse, +place);

		out->type = state[0].getValue();
		state.parse(out).eatOne();

		state.parse(out).with(&out->value, Lower::parse, length - place - 1);

		return out;
	}

	void Assignment::dprint(std::ostream& out) const {
		out << "{";

		out << R"("assigned variables": )";
		nullAwareDprint(variables, out);
		out << R"(, "assignment type": ")" << type.str() << "\"";
		out << R"(, "assigned value": )";
		nullAwareDprint(value, out);

		out << "}";
	}

	void Assignment::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitAssignment(*this);
	}
}
