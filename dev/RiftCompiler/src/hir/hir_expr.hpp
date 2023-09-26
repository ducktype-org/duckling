#pragma once

#include <pst_parser/pst.hpp> // Expr, @TODO: separate expr from rest?
#include <typesystem/typesystem.hpp>
#include <exec/exec.hpp>
#include "analysis_state.hpp"

// This code is a temporary setup
// @Placeholder

namespace hir {

	class Expression;
	
	using ExpressionRef = base::unique_ptr<Expression>;

	class Expression {
	protected:
		Expression(symtable::ScopeRef scope): scope(scope) {};

		bool lookup_done = false;
		bool type_done = false;

		symtable::ScopeRef scope;
		std::optional<ts::TypeDesc<> > type;

	public:
		virtual void lookup(AnalysisState&);
		virtual void determineType(AnalysisState&);

		// in the future this will probably require some „grammar context”
		// in the future this will require AnalysisState to log errors
		static ExpressionRef makeExpr(symtable::ScopeRef scope, pst::ParserCBorrowRef<pst::Expr>);

		// @TODO: this should receive some state:
		virtual ts::TypeDesc<> evalAsType(AnalysisState&);
		virtual exec::CTV eval(AnalysisState&);
		
		ts::TypeDesc<> getType(AnalysisState&);

		virtual ~Expression() = default;
	};

}


