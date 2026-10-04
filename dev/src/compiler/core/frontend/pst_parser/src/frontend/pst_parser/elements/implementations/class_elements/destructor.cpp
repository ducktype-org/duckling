#include "../../hierarchy/class_elements/destructor.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Destructor, body);

	MBox<Destructor> Destructor::parse(LangParserState& state) {
		auto out = makeBox<Destructor>(state);

		CORE_ASSERT(state[0].is(state.getContext()->class_name), "Bad destructor parsing entry");

		PARSE().eatOne();

		PARSE().all(NamedOperator::Period, Keyword::Destroy);

		PARSE().goDown();
		if (state.notEmpty()) state.logInt(makeBox<NonEmptyError>(state.getPosition()));
		PARSE().goUpAndSkip();

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			state.setContextStmt(StmtContext::Normal);
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
