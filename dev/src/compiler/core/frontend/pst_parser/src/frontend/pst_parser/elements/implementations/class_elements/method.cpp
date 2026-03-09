#include "../../hierarchy/class_elements/method.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Method> Method::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<Method>(position, ctx);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		PARSE().all(Keyword::Fun, &out->name, &out->params);
		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

		PARSE().one(NamedOperator::Assign).withDef(&out->body, BlockOrderType::Ordered);

		PST_RETURN out;
	}

	void Method::dprint(std::ostream& out) const {
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

	base::Optional<AccessLocked<ExprHolder>> Method::getRet() const {
		return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
	}

	LangElement::HashAlg& Method::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, name);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Method::acceptVisitor(PstVisitor& visitor) const { visitor.visitMethod(*this); }
}
