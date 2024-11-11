#pragma once

#include <vector>
#include <base/ints.hpp>
#include <base/perfect_hash.hpp>
#include <base/string_id.hpp>

#include <helios/symbols/symbols.hpp>
#include <pst_parser/elements/elements.hpp>
#include <query_framework/query_int.hpp>
#include <typesystem/higher/type_desc.hpp>
#include <typesystem/higher/queries.hpp>

#include "../pst_ref.hpp"
#include "../scope_symbol_id.hpp"
#include "base/box.hpp"
#include "base/exceptions.hpp"
#include "element_ref.hpp"
#include "helios/helios_errors.hpp"
#include "lexer/token_common.hpp"
#include "pst_parser/elements/hierarchy/not_statements.hpp"

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
		// @EXPR: Make a method that returns HResult<tsh::TypeDesc<>, Failed> to avoid panics
		tsh::TypeDesc<> type_desc;

		Expr(ScopeID lifetime_scope, tsh::TypeDesc<> type_desc):
			  lifetime_scope(lifetime_scope),
			  type_desc(type_desc) {}

		virtual ~Expr()                                  = default;
		virtual void debugPrint(std::ostream& out) const = 0;

		virtual void acceptVisitor(HoutExprVisitor&) const = 0;

		static errors::HResult<base::Box<Expr>, errors::Failed>
			fromPST(query::Context& ctx, ScopeID scope, const PstRef<pst::ExprElement> root);

		virtual errors::HResult<i64, errors::Failed> evaluateValue(query::Context& ctx) const = 0;
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
		// @TODO: decide where we handle non-initial value (pre hout/post hout):
		// currently PST always have it.
		base::Optional<ElementRef<Expr>> initial_value;
		tsh::TypeDesc<>                  type;

		// @TODO decide if this is needed:
		SymID helios_symbol;

		VariableStmt(
			ScopeID                          scope,
			base::Optional<ElementRef<Expr>> initial_value,
			tsh::TypeDesc<>                  type,
			SymID                            helios_symbol
		):
			  Stmt(scope),
			  initial_value(std::move(initial_value)),
			  type(type),
			  helios_symbol(helios_symbol) {}

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

		LiteralValueExpr(query::Context& ctx, ScopeID scope, i64 value);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const override;

		errors::HResult<i64, errors::Failed> evaluateValue(query::Context& ctx) const override;
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

		IdentifierExpr(query::Context& ctx, ScopeID scope, SymID symbol);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const override;
	};

	struct BinaryOperatorExpr: public Expr {
		// @TODO: At this point, this should be a symbol.
		//  HOUT should not be concerned with overload resolution.
		lexer::Operator op;

		base::Box<Expr> lhs;
		base::Box<Expr> rhs;

		BinaryOperatorExpr(
			query::Context&  ctx,
			ScopeID          scope,
			lexer::Operator  op,
			base::Box<Expr> lhs,
			base::Box<Expr> rhs
		);

		void debugPrint(std::ostream& out) const final;
		void acceptVisitor(HoutExprVisitor&) const override;

		errors::HResult<i64, errors::Failed> evaluateValue(query::Context& ctx) const override;
	};
}

namespace compiler::helios {
	struct KeyOf_QueryHoutOfExpr {
		PstRef<pst::ExprElement> expr;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	/**
	 * @brief Construct HOUT Expr from Pst Expr, "within" given scope
	 */
	DECLARE_QUERY(QueryHoutOfExpr, KeyOf_QueryHoutOfExpr, code::ElementRef<code::Expr>);
}
