#pragma once

#include <vector>
#include <base/ints.hpp>

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>
#include <base/perfect_hash.hpp>

#include "../pst_ref.hpp"
#include "../scope_symbol_id.hpp"
#include "base/string_id.hpp"
#include "element_ref.hpp"
#include "helios/symbols/symbols.hpp"

namespace compiler::helios::code {

	// @TODO: Add source positions

	class HoutStmtVisitor;
	class HoutExprVisitor;

	/**
	 * @brief Base class for all HOUT statements
	 */
	struct Stmt {
		virtual ~Stmt()                                               = default;
		virtual void debugPrint(usize indent, std::string& out) const = 0;

		virtual void acceptVisitor(HoutStmtVisitor&) const = 0;
	};

	/**
	 * @brief Base class for all HOUT expressions.
	 * All subclasses shall have a "Expr" suffix.
	 */
	struct Expr {
		// @TODO: set/get Type and ValueCategory of Expr
		virtual ~Expr()                                 = default;
		virtual void debugPrint(std::string& out) const = 0;

		virtual void acceptVisitor(HoutExprVisitor&) const = 0;

		static ElementRef<Expr> fromRPN(query::Context& ctx, const rpn::RPNExpr& elements);
	};

	/**
	 * @brief A block of HOUT statements
	 */
	struct CodeBlock final {
		std::vector<ElementRef<Stmt>> statements;
	};

	/* * * * * * * *
	 * Statements: *
	 * * * * * * * */

	/**
	 * @brief Represents `return [expr];` in HOUT
	 */
	struct ReturnStmt final: public Stmt {
		ElementRef<Expr> value;

		ReturnStmt(ElementRef<Expr> value): value(std::move(value)) {}

		void debugPrint(usize indent, std::string& out) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents `return;` in HOUT
	 */
	struct VoidReturnStmt final: public Stmt {
		void debugPrint(usize indent, std::string& out) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents expression statement in HOUT
	 */
	struct ExprStmt final: public Stmt {
		ElementRef<Expr> expr;

		ExprStmt(ElementRef<Expr> expr): expr(std::move(expr)) {}

		void debugPrint(usize indent, std::string& out) const final;
		void acceptVisitor(HoutStmtVisitor&) const override;
	};

	/**
	 * @brief Represents if statement in HOUT
	 */
	struct IfStmt final: public Stmt {
		ElementRef<Expr> condition;
		CodeBlock        body;

		// @TODO: optional else body

		IfStmt(ElementRef<Expr> condition, CodeBlock body):
			  condition(std::move(condition)),
			  body(std::move(body)) {}

		void debugPrint(usize indent, std::string& out) const final;
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

		LiteralValueExpr(i64 value): value(value) {}

		void debugPrint(std::string& out) const final;
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

		IdentifierExpr(SymID symbol): symbol(std::move(symbol)) {}

		void debugPrint(std::string& out) const final;
		void acceptVisitor(HoutExprVisitor&) const override;
	};

	struct BinaryOperatorExpr: public Expr {
		base::StrId op;

		ElementRef<Expr> lhs;
		ElementRef<Expr> rhs;

		BinaryOperatorExpr(base::StrId op, ElementRef<Expr> lhs, ElementRef<Expr> rhs):
			  op(op),
			  lhs(std::move(lhs)),
			  rhs(std::move(rhs)) {}

		void debugPrint(std::string& out) const override;
		void acceptVisitor(HoutExprVisitor&) const override;
	};
}

namespace compiler::helios {
	struct KeyOf_QueryHoutOfExpr {
		ScopeID           scope;
		PstRef<pst::Expr> expr;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	/**
	 * @brief Construct HOUT Expr from Pst Expr, "within" given scope
	 */
	DECLARE_QUERY(QueryHoutOfExpr, KeyOf_QueryHoutOfExpr, code::ElementRef<code::Expr>);
}
