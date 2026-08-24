#include "../../hierarchy/declarations/pattern.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Pattern, name, param, ret, body);

	MBox<Pattern> Pattern::parse(LangParserState& state) {
		auto out = makeBox<Pattern>(state);

		if (!assertStmtChoice<Pattern>(state, state[0].is(Keyword::Pattern))) return nullptr;

		PARSE().all(Keyword::Pattern, &out->name);

		// Patterns take in only one argument, thus we don't use the parametr list and check for
		// braces manually.
		if (!state[0].isBracketGroup(Token::Round)) {
			state.logInt(makeBox<PatternBracketError>(state.getPosition()));
			return nullptr;
		}

		auto bracket_group_token = state[0];
		PARSE().goDown();
		if (state.empty()) {
			state.logInt(makeBox<PatternArgumentCountError>(bracket_group_token.getPosition()));
			return nullptr;
		}

		PARSE().one(&out->param);
		if (!state.empty()) {  // More than one argument.
			state.logInt(makeBox<PatternArgumentCountError>(bracket_group_token.getPosition()));
			return nullptr;
		}

		PARSE().goUpAndSkip();

		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

		PST_RETURN out;
	}

	void Pattern::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		nullAwareDprint(name, out);
		out << ",\"parameter\":";
		nullAwareDprint(param, out);
		out << ",\"return\":";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	HashAlg& Pattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Pattern::acceptVisitor(PstVisitor& visitor) const { visitor.visitPattern(*this); }
}
