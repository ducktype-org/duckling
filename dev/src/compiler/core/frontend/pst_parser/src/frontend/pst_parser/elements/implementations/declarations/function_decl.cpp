// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/declarations/function_decl.hpp"

#include "../../hierarchy/not_statements/code_block_or_statement.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(FunDecl, name, params, ret);

	// @TODO: make better
	MBox<FunDecl> FunDecl::parse(LangParserState& state) {
		auto out = makeBox<FunDecl>(state);

		if (!assertStmtChoice<FunDecl>(state, state[0].is(Keyword::FunDecl))) return nullptr;

		PARSE().all(Keyword::FunDecl);
		out->operator_fixity = parseOperatorFixity(state);
		PARSE().with(&out->name, IdentifierWrapper::parseFunctionName);
		PARSE().one(&out->params);

		if (PARSE().tryEat(NamedOperator::SingleArrow)) PARSE().one(&out->ret);

		PST_RETURN out;
	}

	void FunDecl::dprint(std::ostream& out) const {
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
		out << "}";
	}

	base::Optional<AccessLocked<ExprHolder>> FunDecl::getRet() const {
		return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
	}

	HashAlg& FunDecl::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, operator_fixity);
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void FunDecl::acceptVisitor(PstVisitor& visitor) const { visitor.visitFunDecl(*this); }
}
