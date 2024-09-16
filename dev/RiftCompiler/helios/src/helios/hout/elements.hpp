#pragma once

#include <vector>
#include <base/ints.hpp>
#include <base/perfect_hash.hpp>
#include <base/string_id.hpp>

#include <helios/symbols/symbols.hpp>
#include <pst_parser/elements/elements.hpp>
#include <query_framework/query_impl.hpp>
#include <typesystem/higher/type_desc.hpp>
#include <typesystem/higher/queries.hpp>

#include "../pst_ref.hpp"
#include "../scope_symbol_id.hpp"
#include "element_ref.hpp"

namespace compiler::helios::code {

	// @TODO: Add source positions

	class HoutStmtVisitor;
	class HoutExprVisitor;

	/**
	 * @brief Base class for all HOUT statements
	 */
	struct Stmt {
		ScopeID lifetime_scope;

		Stmt(ScopeID lifetime_scope): lifetime_scope(lifetime_scope) {}

		virtual ~Stmt()                                                    = default;
		virtual void debugPrint(std::ostream& out, usize indent = 0) const = 0;

		virtual void acceptVisitor(HoutStmtVisitor&) const = 0;
	};

	/**
	 * @brief Base class for all HOUT expressions.
	 * All subclasses shall have a "Expr" suffix.
	 */
	struct Expr {
		ScopeID lifetime_scope;

		/**
		 * The type of the expression, and its value category.
		 */
		tsh::TypeDesc<> type_desc;

		Expr(ScopeID lifetime_scope, tsh::TypeDesc<> type_desc):
			  lifetime_scope(lifetime_scope),
			  type_desc(type_desc) {}

		virtual ~Expr()                                                    = default;
		virtual void debugPrint(std::ostream& out, usize indent = 0) const = 0;

		virtual void acceptVisitor(HoutExprVisitor&) const = 0;

		static ElementRef<Expr> fromRPN(query::Context& ctx, const rpn::RPNExpr& elements);
	};

	/**
	 * @brief A block of HOUT statements
	 */
	struct CodeBlock final {
		ScopeID                       lifetime_scope;
		std::vector<ElementRef<Stmt>> statements;
	};

	/* * * * * * * *
	 * Statements: *
	 * * * * * * * */

	 /**
	 * @brief Represents `var/let a : T = ..;` statement in HOUT
	 */
	struct VariableStmt final: public Stmt {
		base::Optional<ElementRef<Expr> > initial_value;
		tsh::TypeDesc<> type;

		// @TODO decide if this is needed:
		SymID helios_symbol;

		VariableStmt(ScopeID scope, base::Optional<ElementRef<Expr> > initial_value, tsh::TypeDesc<> type, SymID helios_symbol):
			Stmt(scope),
			initial_value(std::move(initial_value)),
			type(type),
			helios_symbol(helios_symbol)
		{}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};


	/**
	 * @brief Represents `return [expr];` in HOUT
	 */
	struct ReturnStmt final: public Stmt {
		ElementRef<Expr> value;

		ReturnStmt(ScopeID scope, ElementRef<Expr> value): Stmt(scope), value(std::move(value)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return;` in HOUT
	 */
	struct VoidReturnStmt final: public Stmt {
		VoidReturnStmt(ScopeID scope): Stmt(scope) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents expression statement in HOUT
	 */
	struct ExprStmt final: public Stmt {
		ElementRef<Expr> expr;

		ExprStmt(ScopeID scope, ElementRef<Expr> expr): Stmt(scope), expr(std::move(expr)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents if statement in HOUT
	 */
	struct IfStmt final: public Stmt {
		ElementRef<Expr> condition;
		CodeBlock        body;

		// @TODO: optional else body

		IfStmt(ScopeID scope, ElementRef<Expr> condition, CodeBlock body):
			  Stmt(scope),
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/* * * * * * * * *
	 * Expressions:  *
	 * * * * * * * * */

	/**
	 * @brief Represents a literal value written in the expression.
	 */
	struct LiteralValueExpr final: public Expr {
		// @TODO: ctv + type for consts?
		// @note: this is a mock
		i64 value;

		LiteralValueExpr(ScopeID scope, i64 value, query::Context& ctx):
			  Expr(
				  scope,
				  tsh::TypeDesc<>(
					  // @TODO: Select type of expression based on type of literal.
					  ctx.query<tsh::QueryIntegralType>({ 64 }),
					  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
				  )
			  ),
			  value(value) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutExprVisitor&) const override;
	};

	/**
	 * @brief Represents expression made of a single identifier in HOUT.
	 * @note: This will have to be improved,
	 * when more complex expressions involving "." operator, local variables, etc
	 * will be introduced.
	 */
	struct IdentifierExpr final: public Expr {
		// @note: this is a mock
		SymID symbol;

		IdentifierExpr(ScopeID scope, SymID symbol, query::Context& ctx):
			  Expr(
				  scope,
				  tsh::TypeDesc<>(
					  ctx.query<QueryTypeOfSymbol>(symbol),
					  tsh::ValueCategory(tsh::primaryCategoryOfSymbol(symbol))
				  )
			  ),
			  symbol(std::move(symbol)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutExprVisitor&) const override;
	};

	struct BinaryOperatorExpr: public Expr {
		// @TODO: At this point, this should be a symbol.
		//  HOUT should not be concerned with overload resolution.
		base::StrID op;

		ElementRef<Expr> lhs;
		ElementRef<Expr> rhs;

		BinaryOperatorExpr(
			ScopeID          scope,
			base::StrID      op,
			ElementRef<Expr> lhs,
			ElementRef<Expr> rhs,
			query::Context&  ctx
		):
			  Expr(
				  scope,
				  tsh::TypeDesc<>(
					  // @TODO: Select type of expression based on result type of the operation.
					  ctx.query<tsh::QueryIntegralType>({ 64 }),
					  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
				  )
			  ),
			  op(op),
			  lhs(std::move(lhs)),
			  rhs(std::move(rhs)) {}

		void debugPrint(std::ostream& out, usize indent = 0) const final;
		void acceptVisitor(HoutExprVisitor&) const override;
	};
}

namespace compiler::helios {
	struct KeyOf_QueryHoutOfExpr {
		PstRef<pst::Expr> expr;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	/**
	 * @brief Construct HOUT Expr from Pst Expr, "within" given scope
	 */
	DECLARE_QUERY(QueryHoutOfExpr, KeyOf_QueryHoutOfExpr, code::ElementRef<code::Expr>);
}
