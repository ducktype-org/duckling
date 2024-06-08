#pragma once

#include <vector>
#include <base/ints.hpp>

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>
#include <base/perfect_hash.hpp>

#include "../pst_ref.hpp"
#include "../scope_symbol_id.hpp"
#include "element_ref.hpp"

namespace compiler::helios::code {

	// @TODO: Add source positions

	/**
	 * @brief Base class for all HOUT statements
	 */
	struct Stmt {
		virtual ~Stmt()                                               = default;
		virtual void debugPrint(usize indent, std::string& out) const = 0;
	};

	/**
	 * @brief Base class for all HOUT expressions
	 */
	struct Expr {
		virtual ~Expr()                                 = default;
		virtual void debugPrint(std::string& out) const = 0;
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
	};

	/**
	 * @brief Represents `return;` in HOUT
	 */
	struct VoidReturnStmt final: public Stmt {
		void debugPrint(usize indent, std::string& out) const final;
	};

	/**
	 * @brief Represents expression statement in HOUT
	 */
	struct ExprStmt final: public Stmt {
		ElementRef<Expr> expr;

		ExprStmt(ElementRef<Expr> expr): expr(std::move(expr)) {}

		void debugPrint(usize indent, std::string& out) const final;
	};

	/* * * * * * * * *
	 * Expressions:  *
	 * * * * * * * * */

	/**
	 * @brief Represents integer constant in HOUT
	 */
	struct ConstIntExpr final: public Expr {
		// @TODO: ctv + type for consts?
		// @note: this is a mock
		i64 value;

		ConstIntExpr(i64 value): value{ value } {}

		void debugPrint(std::string& out) const final;
	};

	/**
	 * @brief Represents expression made of single identifier in HOUT
	 * @note: This will have to be improved,
	 * when more complex expressions involving "." operator, local variables, etc
	 * will be introduced.
	 */
	struct IdentifierExpresion final: public Expr {
		// @note: this is a mock
		SymID symbol;

		IdentifierExpresion(SymID symbol): symbol{ std::move(symbol) } {}

		void debugPrint(std::string& out) const final;
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
