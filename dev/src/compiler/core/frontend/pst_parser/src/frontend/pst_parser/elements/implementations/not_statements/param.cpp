#include "../../hierarchy/not_statements/param.hpp"

#include "preamble.hpp"

namespace pst {

	MBox<Param> Param::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Param>(position);

		state.parse(out).all(&out->name, NamedOperator::Colon);

		state.parse(out).one(&out->type);

		if (state.parse(out).tryEat(NamedOperator::Assign)) state.parse(out).one(&out->initial);

		PST_RETURN out;
	}

	base::Optional<AccessLocked<UniversalExprHolder>> Param::getValue() const {
		return initial.map([](const auto& v) { return v.give(); });
	}

	void Param::dprint(std::ostream& out) const {
		out << "{\"Parameter\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(,"type": )";
		nullAwareDprint(type, out);
		if (initial) {
			out << R"(,"initial": )";
			nullAwareDprint(initial.value(), out);
		}

		out << "}}";
	}

	LangElement::HashAlg& Param::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		addToHash(partial_hash, initial.has_value());
		return partial_hash;
	}

	void Param::acceptVisitor(PstVisitor& visitor) const { visitor.visitParam(*this); }
}
