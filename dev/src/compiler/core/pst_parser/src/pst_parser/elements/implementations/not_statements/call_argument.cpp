#include "../../hierarchy/not_statements/call_argument.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<CallArgument> CallArgument::parse(LangParserState& state) {
		auto out = makeBox<CallArgument>(state.getPosition());
		if (state[1].is(lang_def::NamedOperator::Assign)) {
			tpc::Identifier name;
			state.parse(out).one(&name);
                        out->arg_name = std::move(name);
                        state.parse(out).one(lang_def::NamedOperator::Assign);
		}
                state.parse(out).one(&out->arg);
		return out;
	}

	void CallArgument::dprint(std::ostream& out) const {
		// TODO
	}

	void CallArgument::acceptVisitor(PstVisitor& visitor) const {
		//visitor.visitExprValue(*this);
	}
}
