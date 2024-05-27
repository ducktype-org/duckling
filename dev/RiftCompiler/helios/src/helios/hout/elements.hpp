#pragma once

#include <vector>

#include "element_ref.hpp"

namespace compiler::helios::code {

	// @TODO: source positions
	
	struct Stmt {
		// @TODO

		virtual ~Stmt() = default;
		virtual std::string debugPrint() = 0;
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


		std::string debugPrint() final {
			return "return [@TODO]\n";
		}
	};

	struct VReturnStmt final: public Stmt {
		std::string debugPrint() final {
			return "void-return\n";
		}
	};
}

