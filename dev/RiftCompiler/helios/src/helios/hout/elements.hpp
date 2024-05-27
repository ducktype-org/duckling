#pragma once

#include <vector>
#include <base/ints.hpp>

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements.hpp>

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
	};
	
	struct CodeBlock final {
		std::vector<ElementRef<Stmt>> statements;
	};

	/* * * * * * * *
	 * Statements: *
	 * * * * * * * */

	struct ReturnStmt final: public Stmt {
		ElementRef<Expr> value;

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
		i64 value;
	};
}


namespace compiler::helios {
	struct KeyOf_HoutOfExpr {
		ScopeID scope;
		PstRef<pst::Expr> expr;
	};
	
	/**
	 * @brief Construct HOUT Expr from Pst Expr, "within" given scope 
	 * @TODO: perhaps add cache
	 */
	DECLARE_QUERY(HoutOfExpr, KeyOf_HoutOfExpr, code::ElementRef<code::Expr>);
}

