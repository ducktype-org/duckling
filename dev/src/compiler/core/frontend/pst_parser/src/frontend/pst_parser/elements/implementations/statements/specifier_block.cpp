
#include "../../hierarchy/statements/specifier_block.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<SpecifierBlock> SpecifierBlock::parse(LangParserState& state) {
		auto out = makeBox<SpecifierBlock>(state);

		PARSE().withDef(&out->block);

		PST_RETURN out;
	}

	/**
	 * @note For now this will break on escape characters.
	 */
	void SpecifierBlock::dprint(std::ostream& out) const {
		out << "{";

		out << R"("block": )";
		nullAwareDprint(block, out);

		out << "}";
	}

	HashAlg& SpecifierBlock::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	bool SpecifierBlock::trailingSemicolon() { return false; }

	void SpecifierBlock::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitSpecifierBlock(*this);
	}
}
