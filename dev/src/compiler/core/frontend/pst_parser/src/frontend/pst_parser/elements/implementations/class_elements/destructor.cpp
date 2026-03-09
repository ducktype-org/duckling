#include "../../hierarchy/class_elements/destructor.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Destructor> Destructor::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<Destructor>(position, ctx);

		PARSE().eatOne();

		tpc::Identifier ident;
		PARSE().all(NamedOperator::Period, &ident);
		out->kind = ident;

		PARSE().goDown();
		if (state.notEmpty()) state.logInt(makeBox<NonEmptyError>(state.getPosition()));
		PARSE().goUpAndSkip();

		PARSE().one(NamedOperator::Assign).withDef(&out->body, BlockOrderType::Ordered);

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
