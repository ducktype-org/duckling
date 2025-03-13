#pragma once

#include "elements/elements_list.hpp"
#include "pst_parser/access.hpp"
#include <base/exceptions.hpp>

namespace pst {
	// Attention. All of the following [[maybe_unused]] attributes serve purpose of allowing
	// IDEs to generate correct skeleton for defining these methods with a name in place .

	/**
	 * PstExprVisitor is a simple base class for VisitorPattern in `pst::Expr`s.
	 * It is used by calling `expr.acceptExprVisitor(visitor)`.
	 */
	class PstExprVisitor {
	public:
		virtual ~PstExprVisitor() = default;

		virtual void visitPrefixOperator([[maybe_unused]] Access<expr::PrefixOperator> stmt) = 0;

		virtual void visitSuffixOperator([[maybe_unused]] Access<expr::SuffixOperator> stmt) = 0;

		virtual void visitBinaryOperator([[maybe_unused]] Access<expr::BinaryOperator> stmt) = 0;

		virtual void visitExprValue([[maybe_unused]] Access<expr::ExprValue> stmt) = 0;

		virtual void visitTemplateSpecifier([[maybe_unused]] Access<expr::TemplateSpecifier> stmt)
			= 0;

		virtual void visitIdentifierLiteral([[maybe_unused]] Access<expr::IdentifierLiteral> stmt)
			= 0;

		virtual void visitKeywordLiteral([[maybe_unused]] Access<expr::KeywordLiteral> stmt) = 0;

		virtual void visitAccess([[maybe_unused]] Access<expr::Access> stmt) = 0;

		virtual void visitCall([[maybe_unused]] Access<expr::Call> stmt) = 0;

		virtual void visitChainExpr([[maybe_unused]] Access<expr::ChainExpr> stmt) = 0;

		virtual void visitRoundExpr([[maybe_unused]] Access<expr::RoundExpr> stmt) = 0;

		virtual void visitBlockExpr([[maybe_unused]] Access<expr::BlockExpr> stmt) = 0;

		virtual void visitComparisonChain([[maybe_unused]] Access<expr::ComparisonChain> stmt) = 0;

		virtual void visitTernary([[maybe_unused]] Access<expr::Ternary> stmt) = 0;

		virtual void visitComma([[maybe_unused]] Access<expr::Comma> stmt) = 0;

		virtual void visitAssignment([[maybe_unused]] Access<expr::Assignment> stmt) = 0;
	};

	/**
	 * A simple implementation for PstExprVisitor, that by default does nothing on visiting.
	 * It's a helper class, whose functionality is meant to be overriden for desired statements.
	 */
	class PstExprVisitorEmpty: public PstExprVisitor {
	public:
		~PstExprVisitorEmpty() override = default;

		void visitPrefixOperator([[maybe_unused]] Access<expr::PrefixOperator> stmt) override {}

		void visitSuffixOperator([[maybe_unused]] Access<expr::SuffixOperator> stmt) override {}

		void visitBinaryOperator([[maybe_unused]] Access<expr::BinaryOperator> stmt) override {}

		void visitExprValue([[maybe_unused]] Access<expr::ExprValue> stmt) override {}

		void visitTemplateSpecifier([[maybe_unused]] Access<expr::TemplateSpecifier> stmt
		) override {}

		void visitIdentifierLiteral([[maybe_unused]] Access<expr::IdentifierLiteral> stmt
		) override {}

		void visitKeywordLiteral([[maybe_unused]] Access<expr::KeywordLiteral> stmt) override {}

		void visitAccess([[maybe_unused]] Access<expr::Access> stmt) override {}

		void visitCall([[maybe_unused]] Access<expr::Call> stmt) override {}

		void visitChainExpr([[maybe_unused]] Access<expr::ChainExpr> stmt) override {}

		void visitRoundExpr([[maybe_unused]] Access<expr::RoundExpr> stmt) override {}

		void visitBlockExpr([[maybe_unused]] Access<expr::BlockExpr> stmt) override {}

		void visitComparisonChain([[maybe_unused]] Access<expr::ComparisonChain> stmt) override {}

		void visitTernary([[maybe_unused]] Access<expr::Ternary> stmt) override {}

		void visitComma([[maybe_unused]] Access<expr::Comma> stmt) override {}

		void visitAssignment([[maybe_unused]] Access<expr::Assignment> stmt) override {}
	};

/**
 * @brief Macro used to define PstExprVisitor methods
 */
#define PANIC_EXPR_VISITOR_VISIT_METHOD(type)                             \
	void visit##type([[maybe_unused]] Access<expr::type> stmt) override { \
		CORE_PANIC("PstExprVisitorPanicky visited " #type);               \
	}

	/**
	 * A simple implementation for PstExprVisitor, that by default does CORE_PANIC.
	 * It's a helper class, whose functionality is meant to be overriden for desired statements.
	 */
	class PstExprVisitorPanicky: public PstExprVisitor {
	public:
		~PstExprVisitorPanicky() override = default;

		PANIC_EXPR_VISITOR_VISIT_METHOD(PrefixOperator);
		PANIC_EXPR_VISITOR_VISIT_METHOD(SuffixOperator);
		PANIC_EXPR_VISITOR_VISIT_METHOD(BinaryOperator);
		PANIC_EXPR_VISITOR_VISIT_METHOD(ExprValue);
		PANIC_EXPR_VISITOR_VISIT_METHOD(TemplateSpecifier);
		PANIC_EXPR_VISITOR_VISIT_METHOD(IdentifierLiteral);
		PANIC_EXPR_VISITOR_VISIT_METHOD(KeywordLiteral);
		PANIC_EXPR_VISITOR_VISIT_METHOD(Access);
		PANIC_EXPR_VISITOR_VISIT_METHOD(Call);
		PANIC_EXPR_VISITOR_VISIT_METHOD(ChainExpr);
		PANIC_EXPR_VISITOR_VISIT_METHOD(RoundExpr);
		PANIC_EXPR_VISITOR_VISIT_METHOD(BlockExpr);
		PANIC_EXPR_VISITOR_VISIT_METHOD(ComparisonChain);
		PANIC_EXPR_VISITOR_VISIT_METHOD(Ternary);
		PANIC_EXPR_VISITOR_VISIT_METHOD(Comma);
		PANIC_EXPR_VISITOR_VISIT_METHOD(Assignment);
	};
}
