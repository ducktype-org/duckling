// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/expressions/prefix_operator.hpp"

#include "preamble.hpp"

namespace pst::expr {
	CLONE_SUB_ELEMENTS_DEF(PrefixOperator, op, expr);

	void PrefixOperator::dprint(std::ostream& out) const {
		out << "{";

		out << R"("operator": )";
		nullAwareDprint(op, out);
		out << R"(, "expression": )";
		nullAwareDprint(expr, out);

		out << "}";
	}

	void PrefixOperator::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitPrefixOperator(*this);
	}

	HashAlg& PrefixOperator::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	AccessLocked<OperatorWrapper> PrefixOperator::getOperator() const { return op.give(); }

	AccessLocked<ExprElement> PrefixOperator::getExpr() const { return expr.give(); }
}
