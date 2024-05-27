#pragma once

#include <vector>
#include <base/ints.hpp>

#include "element_ref.hpp"


namespace compiler::helios::code {

	// @TODO: source positions
	
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

	struct ReturnStmt final: public Stmt {
		ElementRef<Expr> value;

		void debugPrint(usize indent, std::string& out) const final;
	};

	struct VReturnStmt final: public Stmt {
		void debugPrint(usize indent, std::string& out) const final;
	};
}

