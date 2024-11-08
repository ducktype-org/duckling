#pragma once

#include <lir/lir_structure/function_forward.hpp>
#include <base/box.hpp>

namespace compiler::backend_llvm {
	struct ModuleImpl;
	
	/**
	 * @brief Encapsulates a llvm module in a way
	 * that does not require to include llvm headers.
	 */
	struct Module {
	private:
		// this is done this way, to avoid including llvm headers here:
		Box<ModuleImpl> impl;

	public:
		Module(Box<ModuleImpl> impl): impl(std::move(impl)) {}

		void debugPrint() const;
		
		[[nodiscard]]
		bool verify() const;
	};

	/**
	 * @brief Converts single lir function into a llvm module
	 * containing only this function.
	 * @note This function is a temporary entry point for the llvm backend.
	 */
	Module lirFunctionToModule(const lir::Function&);


	// query for single function into module?
	// query for hout unit into module?
	// what about forward declarations?
}
