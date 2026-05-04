#include "../../hierarchy/declarations/class.hpp"

#include "../../hierarchy/expressions/ternary.hpp"
#include "../../hierarchy/not_statements/class_block.hpp"
#include "preamble.hpp"

namespace pst {
	bool ExprParserHelper::untilExtendsEnd(const TokenStream& state, i64 fwd = 0) {
		return state[fwd].is(Special::Semicolon) || state[fwd].is(NamedOperator::Assign)
		    || internal::Conditions::isImplementsOrBlockGroup(state, fwd);
	}

	MBox<Class> Class::parse(LangParserState& state) {
		auto out = makeBox<Class>(state);

		if (!assertStmtChoice<Class>(state, state[0].is(Keyword::Class))) return nullptr;

		PARSE().all(Keyword::Class, &out->name);

		if (PARSE().tryEat(Keyword::Extends)) PARSE().one(&out->base);
		if (PARSE().tryEat(Keyword::Implements)) PARSE().one(&out->implements);

		PST_NEW_CONTEXT({
			state.setContextClassName(out->name.internal()->unwrap());
			state.setContextBlockOrdering(BlockOrderType::Unordered);
			PARSE().one(&out->body);
		})

		PST_RETURN out;
	}

	void Class::dprint(std::ostream& out) const {
		out << R"({"name":)";
		nullAwareDprint(name, out);

		out << R"(,"Base":)";
		nullAwareDprint(base, out);

		out << R"(,"Implements":)";
		nullAwareDprint(implements, out);

		out << R"(,"body": )";
		nullAwareDprint(body, out);

		out << "}";
	}

	HashAlg& Class::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Class::acceptVisitor(PstVisitor& visitor) const { visitor.visitClass(*this); }
}
