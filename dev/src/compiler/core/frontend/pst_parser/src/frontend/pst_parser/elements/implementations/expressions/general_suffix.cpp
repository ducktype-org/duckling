#include "../../hierarchy/expressions/general_suffix.hpp"

#include "../../hierarchy/expressions/general_prefix.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

#include <base/misc/int_conv.hpp>

namespace pst::expr {
	MBox<ExprElement> GeneralSuffix::parseRecursive(LangParserState& state, u64 iter) {
		if (iter == 0) return Lower::parse(state);

		i64  length = base::safeIntConv<i64>(state.ctokens().size());
		auto out
			= makeBox<GeneralSuffix>(state, state[base::safeIntConv<i64>(length) - 1].getValue());

		PARSE().autoFallbackLen(length - 1).with(&out->expr, parseRecursive, iter - 1);

		PARSE().eatOne();

		PST_RETURN out;
	}

	MBox<ExprElement> GeneralSuffix::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		i64 fwd            = 0;
		i64 reduced_length = base::safeIntConv<i64>(length);
		// Here this should include the prefix word operators in the future
		PST_WHILE(fwd < length && state[fwd].isPrefixOperator()) fwd++;
		PST_WHILE(fwd < reduced_length && state[reduced_length - 1].isOperatorSymbol())
		reduced_length--;
		if (fwd == reduced_length) {}  // Error

		if (fwd + 1 < reduced_length && state[reduced_length - 1].isIdentifier()
		    && !state[reduced_length - 2]
		            .asBinaryOperator()
		            .map([](auto x) { return x.isAccessOp(); })
		            .copyValueOr(false))
			reduced_length--;
		return parseRecursive(state, base::safeIntConv<u64>(length - reduced_length));
	}
}
