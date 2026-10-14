// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/declarations/function.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Fun, name, params, ret, body);

	// @TODO: make better
	MBox<Fun> Fun::parse(LangParserState& state) {
		auto out = makeBox<Fun>(state);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		PARSE().all(Keyword::Fun);
		out->operator_fixity = parseOperatorFixity(state);
		PARSE().with(&out->name, IdentifierWrapper::parseFunctionName);
		PARSE().one(&out->params);

		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

		PST_RETURN out;
	}

	base::Optional<AccessLocked<ExprHolder>> Fun::getRet() const {
		return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
	}

	void Fun::dprint(std::ostream& out) const {
		out << "{";
		if (operator_fixity != OperatorFixity::None) {
			out << R"("operator_fixity":")"
				<< (operator_fixity == OperatorFixity::Prefix ? "prefix" : "suffix") << "\",";
		}
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

	HashAlg& Fun::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, operator_fixity);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Fun::acceptVisitor(PstVisitor& visitor) const { visitor.visitFun(*this); }
}
