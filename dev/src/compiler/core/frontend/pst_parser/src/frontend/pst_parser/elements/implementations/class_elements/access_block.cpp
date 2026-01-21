#include "../../hierarchy/class_elements/access_block.hpp"

#include "../../hierarchy/not_statements/class_block.hpp"
#include "preamble.hpp"

namespace pst {

	LangElement::HashAlg& AccessBlock::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, specifier);
		return partial_hash;
	}

	MBox<AccessBlock> AccessBlock::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<AccessBlock>(position, ctx);

		if (not ACCESS_SPECIFIERS.contains(state[0].asKeyword())) {
			state.logInt(makeBox<NoSpecifierError>(position));
		} else {
			out->context.specifiers.emplace_back(&state[0]);
			out->specifier = state[0].asKeyword();
		}

		// Consume the keyword specifying the visibility
		state.parse(out).eatOne();

		state.parse(out).with(&out->block, ClassBlock::parse, out->getContext());

		PST_RETURN out;
	}

	void AccessBlock::dprint(std::ostream& out) const {
		out << "{";

		out << R"("specifier": )";
		tpc::nullAwareDprint(specifier, out);
		out << R"(, "code block": )";
		nullAwareDprint(block, out);

		out << "}";
	}

	void AccessBlock::acceptVisitor(PstVisitor& visitor) const { visitor.visitAccessBlock(*this); }
}
