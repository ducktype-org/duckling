#include "../../hierarchy/not_statements/call_argument.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(CallArgument, arg_name, arg);

	MBox<CallArgument> CallArgument::parse(LangParserState& state) {
		auto out = makeBox<CallArgument>(state);
		if (state[1].is(lang_def::NamedOperator::Assign)) {
			out->arg_name.emplace();
			PARSE().one(&out->arg_name.value());
			PARSE().one(lang_def::NamedOperator::Assign);
		}

		PARSE().one(&out->arg);
		PST_RETURN out;
	}

	void CallArgument::dprint(std::ostream& out) const {
		out << "{";
		if (arg_name.has_value()) {
			out << R"("name": )";
			nullAwareDprint(arg_name.value(), out);
			out << ",";
		}
		out << R"("value":)";
		nullAwareDprint(arg, out);
		out << "}";
	}

	HashAlg& CallArgument::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void CallArgument::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitCallArgument(*this);
	}
}
