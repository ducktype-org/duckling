#include "../../../hierarchy/not_statements/patterns/binding_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<BindingPattern> BindingPattern::parse(LangParserState& state) {
		if (!state[0].isIdentifier()) return nullptr;
		auto position = state.getPosition();
		auto out      = makeBox<BindingPattern>(position);
		state.parse(out).one(&out->name);
		return out;
	}

	void BindingPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "binding",)";
		out << R"("name": )";
		nullAwareDprint(name, out);
		out << "}";
	}

	LangElement::HashAlg& BindingPattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		return partial_hash;
	}

	void BindingPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitBindingPattern(*this);
	}
}
