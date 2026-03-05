#include "../../hierarchy/class_elements/destructor.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Destructor> Destructor::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Destructor>(position);

		state.parse(out).eatOne();

		tpc::Identifier ident;
		state.parse(out).all(NamedOperator::Period, &ident);
		out->kind = ident;

		state.parse(out).goDown();
		if (state.notEmpty()) state.logInt(makeBox<NonEmptyError>(state.getPosition()));
		state.parse(out).goUpAndSkip();

		state.parse(out).fallbackLen(state.ctokens().size());

		state.setConstextBlockOrdering(BlockOrderType::Ordered);
		state.parse(out).all(NamedOperator::Assign, &out->body);

		state.parse(out).exitFallback();

		PST_RETURN out;
	}

	void Destructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Destructor::acceptVisitor(PstVisitor& visitor) const { visitor.visitDestructor(*this); }
}
