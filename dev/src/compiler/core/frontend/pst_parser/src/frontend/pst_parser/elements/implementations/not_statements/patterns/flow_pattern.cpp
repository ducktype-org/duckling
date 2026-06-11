#include "../../../hierarchy/not_statements/patterns/flow_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(FlowPattern, pattern, as_identifier, type_constraint);

	MBox<FlowPattern> FlowPattern::parse(LangParserState& state) {
		auto out = makeBox<FlowPattern>(state);

		PARSE().one(&out->pattern);

		if (PARSE().tryEat(Keyword::As)) {
			out->as_identifier.emplace();
			PARSE().one(&out->as_identifier.value());
		}

		if (PARSE().tryEat(NamedOperator::Colon)) PARSE().one(&out->type_constraint);
		PST_RETURN out;
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

	base::Optional<AccessLocked<UniversalExprHolder>> FlowPattern::getTypeConstraint() const {
		return type_constraint.map([](const auto& value) { return value.give(); });
	}

	HashAlg& FlowPattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, as_identifier.has_value());
		addToHash(partial_hash, type_constraint.has_value());
		return partial_hash;
	}

	void FlowPattern::acceptVisitor(PstVisitor& visitor) const {
		return visitor.visitFlowPattern(*this);
	}
}
