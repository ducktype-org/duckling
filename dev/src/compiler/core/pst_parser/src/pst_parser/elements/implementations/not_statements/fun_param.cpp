#include "../../hierarchy/not_statements/fun_param.hpp"

#include "preamble.hpp"

namespace pst {

	MBox<FunParam> FunParam::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<FunParam>(position);

		state.parse(out).all(&out->name, NamedOperator::Colon);

		state.parse(out).one(&out->type);

		if (state.parse(out).tryEat(NamedOperator::Assign)) state.parse(out).one(&out->initial);

		return out;
	}

	base::Optional<AccessLocked<UniversalExprHolder>> FunParam::getValue() const {
		return initial.map([](const auto& v) { return v.give(); });
	}

	void FunParam::dprint(std::ostream& out) const {
		out << "{\"Function Parameter\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(,"type": )";
		nullAwareDprint(type, out);
		if (initial) {
			out << R"(,"initial": )";
			nullAwareDprint(initial.value(), out);
		}

		out << "}}";
	}

	LangElement::HashAlg& FunParam::calcStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		addToHash(partial_hash, initial.has_value());
		return partial_hash;
	}

	void FunParam::acceptVisitor(PstVisitor& visitor) const { visitor.visitFunParam(*this); }
}
