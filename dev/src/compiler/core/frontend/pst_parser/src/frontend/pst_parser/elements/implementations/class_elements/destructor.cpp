#include "../../hierarchy/class_elements/destructor.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Destructor> Destructor::parse(LangParserState& state) {
		auto out = makeBox<Destructor>(state);

		PARSE().eatOne();

		tpc::Identifier ident;
		PARSE().all(NamedOperator::Period, &ident);
		out->kind = ident;

		PARSE().goDown();
		if (state.notEmpty()) state.logInt(makeBox<NonEmptyError>(state.getPosition()));
		PARSE().goUpAndSkip();

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

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
