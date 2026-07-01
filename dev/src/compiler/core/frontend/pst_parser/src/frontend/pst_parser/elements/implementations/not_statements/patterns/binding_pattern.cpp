#include "../../../hierarchy/not_statements/patterns/binding_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(BindingPattern, name);

	MBox<BindingPattern> BindingPattern::parse(LangParserState& state) {
		auto out = makeBox<BindingPattern>(state);
		PARSE().one(&out->name);
		PST_RETURN out;
	}

	void BindingPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "binding",)";
		out << R"("name": )";
		nullAwareDprint(name, out);
		out << "}";
	}

	HashAlg& BindingPattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void BindingPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitBindingPattern(*this);
	}
}
