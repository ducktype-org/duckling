#pragma once

#include <vector>

#include "element_ref.hpp"

namespace compiler::helios::code {

	// @TODO: source positions
	
	struct Stmt { };

	struct Expr { };
	
	struct CodeBlock final {
		std::vector<ElementRef<Stmt>> statements;
	};

	struct ReturnStmt final: public Stmt {
		ElementRef<Expr> value;
	};

	struct VReturnStmt final: public Stmt {	};
}

