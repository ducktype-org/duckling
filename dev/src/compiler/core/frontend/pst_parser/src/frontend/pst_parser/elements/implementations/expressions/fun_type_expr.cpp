// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/fun_type_expr.hpp"

#include "../../hierarchy/expressions/general_prefix.hpp"  // IWYU pragma: keep
#include "../../hierarchy/expressions/ternary.hpp"         // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(FunTypeExpr, params, ret);

	MBox<ExprElement> FunTypeExpr::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		auto out = makeBox<FunTypeExpr>(state);
		PARSE().one(Keyword::Fun).goDown();
		u64              length = state.ctokens().size();
		std::vector<i64> ends;
		for (i64 i = 0; i < length; i++)
			if (state[i].is(Special::Comma)) ends.push_back(i);

		i64 start = -1;
		for (auto end: ends) {
			out->params.emplace_back();
			PARSE().autoFallbackLen(end - 1 - start).with(&out->params.back(), Ternary::parse);
			state.parse(out).one(Special::Comma);
			start = end;
		}
		if (start + 1 != length) {
			out->params.emplace_back();
			PARSE().with(&out->params.back(), Ternary::parse);
		}
		PARSE().goUpAndSkip();

		if (PARSE().tryEat(NamedOperator::SingleArrow))
			PARSE().with(&out->ret, GeneralPrefix::parse);

		PST_RETURN out;
	}

	void FunTypeExpr::dprint(std::ostream& out) const {
		out << "{";
		out << R"("parameters": [)";
		bool first = true;
		for (auto& param: params) {
			if (!first)
				out << ", ";
			else
				first = false;
			nullAwareDprint(param, out);
		}
		out << "]";
		out << R"(, "return": )";
		if (ret)
			nullAwareDprint(ret.value(), out);
		else
			out << "\"unit\"";
		out << "}";
	}

	HashAlg& FunTypeExpr::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, params.size());
		addToHash(partial_hash, ret.has_value());
		return partial_hash;
	}

	void FunTypeExpr::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitFunTypeExpr(*this);
	}

	base::Optional<AccessLocked<ExprElement>> FunTypeExpr::getRet() const {
		return ret.map([](const auto& v) -> AccessLocked<ExprElement> { return v.give(); });
	}

	void FunTypeExpr::calcElementPathHashRecursive() {
		auto path = getElementPathHash();
		calcIndexedListChildPath<ExprElement>({ params }, path);
		if (ret) calcNamedChildPath(ret.value(), path);
	}
}
