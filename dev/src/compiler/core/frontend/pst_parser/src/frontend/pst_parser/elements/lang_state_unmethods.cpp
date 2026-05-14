#include "lang_state_unmethods.hpp"

#include "../lang_parser_state.hpp"
#include "hierarchy/expr_holders.hpp"
#include "hierarchy/not_statements/expr_element.hpp"  // IWYU pragma: keep
#include "implementations/preamble.hpp"

namespace pst::internal {
	dia::SourcePosition getPosition(const LangParserState& state) { return state.getPosition(); }

	HashType getContextHash(const LangParserState& state) {
		HashAlg partial_hash;
		addToHash(partial_hash, *state.getContext());
		return partial_hash.finalize();
	}

	void parseExprIntoHolder(
		LangParserState& state, Ref<ExprHolder> out, ExprParseFun parse_fun, u64 length
	) {
		PARSE().autoFallbackLen(length).with(&out->expr, parse_fun);
	}

	bool isSentinel(LangParserState& state, i64 fwd) {
		return state[fwd].is(lexer::Token::Type::Sentinel);
	}

	const TokenStream& getTokenStream(LangParserState& state) { return state.ctokens(); }

	u64 streamSize(LangParserState& state) { return state.ctokens().size(); }

	bool isGood(LangParserState& state) { return not state.int_err->hasErrors(); }
}
