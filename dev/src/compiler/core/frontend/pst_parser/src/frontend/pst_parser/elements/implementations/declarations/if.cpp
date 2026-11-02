// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/declarations/if.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "../../hierarchy/not_statements/round_group_expression.hpp"   // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(If, name, condition, then_body, else_body);

	MBox<If> If::parse(LangParserState& state) {
		auto out = makeBox<If>(state);

		if (!assertStmtChoice<If>(state, state[0].is(Keyword::If))) return nullptr;

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().one(Keyword::If);
			if (PARSE().tryEat(Keyword::Const)) out->is_const = true;
			PARSE().all(&out->name, &out->condition, &out->then_body);

			if (PARSE().tryEat(Keyword::Else)) PARSE().one(&out->else_body);
		})

		PST_RETURN out;
	}

	void If::dprint(std::ostream& out) const {
		out << "{";

		if (is_const) out << R"("is_const":true,)";

		if (name.has_value()) {
			out << "\"name\":";
			nullAwareDprint(name.value(), out);
			out << ",";
		}
		out << "\"condition\":";
		nullAwareDprint(condition, out);
		out << ",\"then body\":";
		nullAwareDprint(then_body, out);
		if (else_body) {
			out << ",\"else body\":";
			nullAwareDprint(else_body.value(), out);
		}

		out << "}";
	}

	HashAlg& If::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, else_body.has_value());
		addToHash(partial_hash, is_const);
		return partial_hash;
	}

	base::Optional<AccessLocked<ExprHolder>> If::getCondition() const {
		const auto condition_group = condition.internal().toOpt();

		if (!condition_group.has_value()) return {};

		return condition_group.value()->getExpr();
	}

	void If::acceptVisitor(PstVisitor& visitor) const { visitor.visitIf(*this); }
}
