#pragma once

#include <lir/lir_structure/function_forward.hpp>
#include <base/box.hpp>

namespace compiler::backend_llvm {

	void init();

	struct ModuleImpl;

	struct Module {
	private:
		// this is done this way, to avoid including llvm headers here:
		Box<ModuleImpl> impl;

	public:

		void debugPrint(std::ostream&) const;
		
		[[nodiscard]]
		bool verify() const;
	};

	/**
	 * @brief Converts single lir function into a llvm module
	 * containing only this function.
	 * @note This function is a temporary entry point for the llvm backend.
	 */
	Module llvmPrintLir(const lir::Function&);


	// query for single function into module?
	// query for hout unit into module?
	// what about forward declarations?
}
