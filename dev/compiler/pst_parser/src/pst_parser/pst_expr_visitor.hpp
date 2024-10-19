#pragma once

#include "elements/elements_list.hpp"
#include <base/exceptions.hpp>

namespace pst {
	// Attention. All of the following [[maybe_unused]] attributes serve purpose of allowing
	// IDEs to generate correct skeleton for defining these methods with a name in place .

	/**
	 * PstStmtVisitor is a simple base class for VisitorPattern in `pst::Stmt`s.
	 * It is used by calling `stmt.acceptVisitor(visitor)`.
	 */
	class PstExprVisitor {
	public:
		virtual ~PstExprVisitor() = default;

		virtual void visitPrefixOperator([[maybe_unused]] const expr::PrefixOperator& stmt) = 0;

		virtual void visitSuffixOperator([[maybe_unused]] const expr::SuffixOperator& stmt) = 0;

		virtual void visitBinaryOperator([[maybe_unused]] const expr::BinaryOperator& stmt) = 0;

		virtual void visitExprValue([[maybe_unused]] const expr::ExprValue& stmt) = 0;

		virtual void visitTemplateSpecifier([[maybe_unused]] const expr::TemplateSpecifier& stmt) = 0;

		virtual void visitIdentifierLiteral([[maybe_unused]] const expr::IdentifierLiteral& stmt) = 0;

		virtual void visitAccess([[maybe_unused]] const expr::Access& stmt) = 0;

		virtual void visitCall([[maybe_unused]] const expr::Call& stmt) = 0;

		virtual void visitChainExpr([[maybe_unused]] const expr::ChainExpr& stmt) = 0;

		virtual void visitRoundExpr([[maybe_unused]] const expr::RoundExpr& stmt) = 0;

		virtual void visitBlockExpr([[maybe_unused]] const expr::BlockExpr& stmt) = 0;

		virtual void visitComparisonChain([[maybe_unused]] const expr::ComparisonChain& stmt) = 0;

		virtual void visitTernary([[maybe_unused]] const expr::Ternary& stmt) = 0;

		virtual void visitComma([[maybe_unused]] const expr::Comma& stmt) = 0;

		virtual void visitAssignment([[maybe_unused]] const expr::Assignment& stmt) = 0;
	};

	/**
	 * A simple implementation for PstStmtVisitor, that by default does nothing on visiting.
	 * It's a helper class, whose functionality is meant to be overriden for desired statements.
	 */
	class PstExprVisitorEmpty: public PstExprVisitor {
	public:
		~PstExprVisitorEmpty() override = default;

		void visitPrefixOperator([[maybe_unused]] const expr::PrefixOperator& stmt) override {};

		void visitSuffixOperator([[maybe_unused]] const expr::SuffixOperator& stmt) override {};

		void visitBinaryOperator([[maybe_unused]] const expr::BinaryOperator& stmt) override {};

		void visitExprValue([[maybe_unused]] const expr::ExprValue& stmt) override {};

		void visitTemplateSpecifier([[maybe_unused]] const expr::TemplateSpecifier& stmt) override {};

		void visitIdentifierLiteral([[maybe_unused]] const expr::IdentifierLiteral& stmt) override {};

		void visitAccess([[maybe_unused]] const expr::Access& stmt) override {};

		void visitCall([[maybe_unused]] const expr::Call& stmt) override {};

		void visitChainExpr([[maybe_unused]] const expr::ChainExpr& stmt) override {};

		void visitRoundExpr([[maybe_unused]] const expr::RoundExpr& stmt) override {};

		void visitBlockExpr([[maybe_unused]] const expr::BlockExpr& stmt) override {};

		void visitComparisonChain([[maybe_unused]] const expr::ComparisonChain& stmt) override {};

		void visitTernary([[maybe_unused]] const expr::Ternary& stmt) override {};

		void visitComma([[maybe_unused]] const expr::Comma& stmt) override {};

		void visitAssignment([[maybe_unused]] const expr::Assignment& stmt) override {};
	};

/**
 * @brief Macro used to define PstExprVisitor methods
 */
#define PANIC_EXPR_VISITOR_VISIT_METHOD(type)                           \
	void visit##type([[maybe_unused]] const expr::type& stmt) override { \
		CORE_PANIC("PstExprVisitorPanicky visited " #type);        \
	}

	/**
	 * A simple implementation for PstStmtVisitor, that by default does CORE_PANIC.
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
