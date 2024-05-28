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
	
	struct Stmt {
		// @TODO

		virtual ~Stmt() = default;
		virtual void debugPrint(usize indent, std::string& out) const = 0;
	};

	struct Expr { 
		// @TODO

		virtual ~Expr() = default;
		virtual void debugPrint(std::string& out) const = 0;
	};
	
	struct CodeBlock final {
		std::vector<ElementRef<Stmt>> statements;
	};

	/* * * * * * * *
	 * Statements: *
	 * * * * * * * */

	struct ReturnStmt final: public Stmt {
		ElementRef<Expr> value;

		ReturnStmt(ElementRef<Expr> value): value(std::move(value)) {}

		void debugPrint(usize indent, std::string& out) const final;
	};

	struct VReturnStmt final: public Stmt {
		void debugPrint(usize indent, std::string& out) const final;
	};

	/* * * * * * * * *
	 * Expressions:  *
	 * * * * * * * * */

	struct ConstIntExpr final: public Expr {
		// @TODO: ctv + type for consts?
		// @note: this is a mock
		i64 value;
		ConstIntExpr(i64 value): value{value} {}
		void debugPrint(std::string& out) const final;
	};

	struct IdentifierExpresion final: public Expr {
		// @note: this is a mock
		SymID symbol;
		IdentifierExpresion(SymID symbol): symbol{symbol} {}
		void debugPrint(std::string& out) const final;
	};
}


namespace compiler::helios {
	struct KeyOf_QueryHoutOfExpr {
		ScopeID scope;
		PstRef<pst::Expr> expr;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};
	
	/**
	 * @brief Construct HOUT Expr from Pst Expr, "within" given scope 
	 * @TODO: perhaps add cache
	 */
	DECLARE_QUERY(QueryHoutOfExpr, KeyOf_QueryHoutOfExpr, code::ElementRef<code::Expr>);
}

