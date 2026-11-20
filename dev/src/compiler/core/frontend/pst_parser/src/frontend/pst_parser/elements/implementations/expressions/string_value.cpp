// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/string_value.hpp"

#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> ExprStrValue::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].isString()) {
			// This should (probably) never happen with how it's called by the parser
			state.logInt(makeBox<BadStrValueError>(pos));
			return nullptr;
		}

		auto out = makeBox<ExprStrValue>(state, state[0].getValue());
		PARSE().eatOne();

		if (length > 1) state.logInt(makeBox<MoreThanStrValueError>(pos));

		PST_RETURN out;
	}

	void ExprStrValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("string": ")" << string.value.str() << "\"";

		out << "}";
	}

	HashAlg& ExprStrValue::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, string);
		return partial_hash;
	}

	void ExprStrValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprStrValue(*this);
	}
}
