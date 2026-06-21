#include "../../hierarchy/class_elements/method.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Method, name, params, ret, body);

	MBox<Method> Method::parse(LangParserState& state) {
		auto out = makeBox<Method>(state);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		PARSE().all(Keyword::Fun, &out->name, &out->params);
		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

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

	HashAlg& Method::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Method::acceptVisitor(PstVisitor& visitor) const { visitor.visitMethod(*this); }
}
