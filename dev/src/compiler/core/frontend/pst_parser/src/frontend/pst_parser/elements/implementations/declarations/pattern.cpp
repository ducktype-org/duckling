#include "../../hierarchy/declarations/pattern.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {

	MBox<Pattern> Pattern::parse(LangParserState& state) {
		auto out = makeBox<Pattern>(state);

		if (!assertStmtChoice<Pattern>(state, state[0].is(Keyword::Pattern))) return nullptr;

		state.parse(out).all(Keyword::Pattern, &out->name);

		// Patterns take in only one argument, thus we don't use the parametr list and check for
		// braces manually.
		if (!state[0].isBracketGroup(Token::Round)) {
			state.logInt(makeBox<PatternBracketError>(state.getPosition()));
			return nullptr;
		}

		auto bracket_group_token = state[0];
		state.parse(out).goDown();
		if (state.empty()) {
			state.logInt(makeBox<PatternArgumentCountError>(bracket_group_token.getPosition()));
			return nullptr;
		}

		state.parse(out).one(&out->param);
		if (!state.empty()) {  // More than one argument.
			state.logInt(makeBox<PatternArgumentCountError>(bracket_group_token.getPosition()));
			return nullptr;
		}

		state.parse(out).goUpAndSkip();

		if (state.parse(out).tryEat(NamedOperator::SingleArrow)) state.parse(out).one(&out->ret);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			state.parse(out).all(NamedOperator::Assign, &out->body);
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
		addToHash(partial_hash, name);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Pattern::acceptVisitor(PstVisitor& visitor) const { visitor.visitPattern(*this); }
}
