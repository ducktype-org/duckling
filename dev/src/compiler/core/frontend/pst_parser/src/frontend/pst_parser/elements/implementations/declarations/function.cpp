#include "../../hierarchy/declarations/function.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	// @TODO: make better
	MBox<Fun> Fun::parse(LangParserState& state) {
		auto position = state.getPosition();
		auto out      = makeBox<Fun>(position);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		PARSE().all(Keyword::Fun, &out->name);
		PARSE().one(&out->params);

		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

		PARSE().one(NamedOperator::Assign).withDef(&out->body, BlockOrderType::Ordered);

		PST_RETURN out;
	}

	base::Optional<AccessLocked<ExprHolder>> Fun::getRet() const {
		return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
	}

	void Fun::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"parameters\":";
		nullAwareDprint(params, out);
		out << ",\"return\":";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	LangElement::HashAlg& Fun::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Fun::acceptVisitor(PstVisitor& visitor) const { visitor.visitFun(*this); }
}
