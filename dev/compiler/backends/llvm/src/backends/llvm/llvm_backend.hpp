#pragma once

#include <lir/lir_structure/lir_structure.hpp>

namespace compiler::backend::llvm_backend {

	struct Module;
	struct Function;

	// temporary for testing:
	void llvmPrintLir(const lir::Function&);


	// query for single function into module?
	// query for hout unit into module?
	// what about forward declarations?


}