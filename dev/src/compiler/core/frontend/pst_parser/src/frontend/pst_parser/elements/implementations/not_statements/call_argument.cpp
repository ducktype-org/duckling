#include "../../hierarchy/not_statements/call_argument.hpp"

#include "preamble.hpp"

namespace pst {
	MBox<CallArgument> CallArgument::parse(LangParserState& state) {
		auto out = makeBox<CallArgument>(state);
		if (state[1].is(lang_def::NamedOperator::Assign)) {
			state.parse(out).one(&out->arg_name);
			state.parse(out).one(lang_def::NamedOperator::Assign);
		}

		state.parse(out).one(&out->arg);
		PST_RETURN out;
	}

	void CallArgument::dprint(std::ostream& out) const {
		out << "{";
		out << R"("name": )";
		nullAwareDprint(arg_name, out);
		out << R"(, "value":)";
		nullAwareDprint(arg, out);
		out << "}";
	}

	LangElement::HashAlg& CallArgument::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, arg_name);
		return partial_hash;
	}
}
