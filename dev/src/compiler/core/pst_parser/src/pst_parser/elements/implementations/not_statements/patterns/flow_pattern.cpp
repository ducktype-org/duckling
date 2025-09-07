#include "../../../hierarchy/not_statements/patterns/flow_pattern.hpp"
#include "../preamble.hpp"

namespace pst {
	MBox<FlowPattern> FlowPattern::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<FlowPattern>(position);

		state.parse(out).one(&out->pattern);

		if (state.parse(out).tryEat(Keyword::As)) {
			tpc::Identifier temp_ident;
			state.parse(out).one(&temp_ident);
			out->as_identifier = temp_ident;
		}

		if (state.parse(out).tryEat(NamedOperator::Colon))
			state.parse(out).one(&out->type_constraint);
		return out;
	}

	void FlowPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "flow",)";
		out << R"("analysis_pattern": )";
		nullAwareDprint(pattern, out);

		if (as_identifier) {
			out << ", ";
			out << R"("as_identifier": )";
			nullAwareDprint(*as_identifier, out);
		}
		if (type_constraint) {
			out << ", ";
			out << R"("type_constraint": )";
			nullAwareDprint(*type_constraint, out);
		}
		out << "}";
	}

	void FlowPattern::acceptVisitor(PstVisitor& visitor) const {
		return visitor.visitFlowPattern(*this);
	}
}
