// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/access.hpp"

#include "expressions_errors.hpp"
#include "preamble.hpp"

namespace pst::expr {

	CLONE_SUB_ELEMENTS_DEF(Access, type, name);

	MBox<ExprElement> Access::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		if (length != 2) {
			state.logInt(makeBox<BadAccessError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		auto out = makeBox<Access>(state);

		// This should never occur if access parsing is called well
		if (!state[0].asBinaryOperator().map([](auto x) { return x.isAccessOp(); }
		    ).copyValueOr(false)) {
			state.logInt(makeBox<BadAccessError>(
				dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd())
			));
		}

		PARSE().all(&out->type, &out->name);

		PST_RETURN out;
	}

	void Access::dprint(std::ostream& out) const {
		out << "{";

		out << R"("type": )";
		nullAwareDprint(type, out);
		out << R"(, "name": )";
		nullAwareDprint(name, out);

		out << "}";
	}

	HashAlg& Access::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Access::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitAccess(*this); }
}
