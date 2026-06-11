#pragma once

#include "context.hpp"
#include "ids.hpp"

#include <frontend/pst_parser/elements/elements_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <frontend/pst_parser/pst_expr_visitor.hpp>

namespace pg {
	/// Result of compile-time evaluation: raw 64-bit data plus the type telling
	/// how to interpret it (i64 or bool for now).
	struct ConstValue {
		u64        bits{ 0 };
		tp::TypeID type_id{};
	};

	/**
	 * @brief Evaluates a PST expression at compile time.
	 *
	 * Supports numeric/boolean literals, parentheses, arithmetic and logical binary
	 * operators, comparisons, references to other constants and namespace-qualified
	 * constants (`ns::CONST`).
	 */
	class ConstEval final: public pst::expr::PstExprVisitorPanicky {
	public:
		ConstEval(Context& ctx): ctx(ctx) {}

		ConstValue eval(const pst::ExprElement& expr);

		void visitExprNumericValue(pst::Access<pst::expr::ExprNumericValue> lit) override;
		void visitKeywordLiteral(pst::Access<pst::expr::KeywordLiteral> kw) override;
		void visitRoundExpr(pst::Access<pst::expr::RoundExpr> round) override;
		void visitBinaryOperator(pst::Access<pst::expr::BinaryOperator> op) override;
		void visitComparisonChain(pst::Access<pst::expr::ComparisonChain> chain) override;
		void visitIdentifierLiteral(pst::Access<pst::expr::IdentifierLiteral> ident) override;
		void visitChainExpr(pst::Access<pst::expr::ChainExpr> chain) override;

	private:
		ConstValue constantFromSymbol(sym::SymbolID sym_id, base::StrID name);

		Context& ctx;

		ConstValue result{};
	};

	/// Evaluates a `const` declaration and registers it as a ConstantSymbol in the
	/// current namespace. Shared by GlobalPass (top-level consts) and FunctionPass
	/// (consts local to a function body).
	void registerConstSymbol(Context& ctx, pst::Access<pst::Const> stmt);
}
