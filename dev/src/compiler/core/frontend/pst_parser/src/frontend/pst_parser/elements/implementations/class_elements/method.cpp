// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/class_elements/method.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Method, name, params, ret, body);

	MBox<Method> Method::parse(LangParserState& state) {
		auto out = makeBox<Method>(state);

		if (!assertStmtChoice<Fun>(state, state[0].is(Keyword::Fun))) return nullptr;

		PARSE().all(Keyword::Fun);
		if (PARSE().tryEat(Keyword::Prefix))
			out->operator_fixity = OperatorFixity::Prefix;
		else if (PARSE().tryEat(Keyword::Suffix))
			out->operator_fixity = OperatorFixity::Suffix;
		PARSE().with(&out->name, IdentifierWrapper::parseFunctionName);
		PARSE().one(&out->params);
		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			state.setContextStmt(StmtContext::Normal);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

		PST_RETURN out;
	}

	void Method::dprint(std::ostream& out) const {
		out << "{";
		if (operator_fixity != OperatorFixity::None) {
			out << "\"operator_fixity\":\""
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

	base::Optional<AccessLocked<ExprHolder>> Method::getRet() const {
		return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
	}

	HashAlg& Method::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, operator_fixity);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void Method::acceptVisitor(PstVisitor& visitor) const { visitor.visitMethod(*this); }
}
