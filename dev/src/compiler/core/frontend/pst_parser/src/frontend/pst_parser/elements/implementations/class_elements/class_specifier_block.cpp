#include "../../hierarchy/class_elements/class_specifier_block.hpp"

#include "../../hierarchy/not_statements/class_block.hpp"
#include "preamble.hpp"

namespace pst {
	HashAlg& ClassSpecifierBlock::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	MBox<ClassSpecifierBlock> ClassSpecifierBlock::parse(LangParserState& state) {
		auto out = makeBox<ClassSpecifierBlock>(state);

		PARSE().one(&out->block);

		PST_RETURN out;
	}

	void ClassSpecifierBlock::dprint(std::ostream& out) const {
		out << "{";

		out << R"("code block": )";
		nullAwareDprint(block, out);

		out << "}";
	}

	void ClassSpecifierBlock::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitClassSpecifierBlock(*this);
	}
}
