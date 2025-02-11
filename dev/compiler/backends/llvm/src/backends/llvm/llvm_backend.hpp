#pragma once

#include <lir/lir_structure/function_forward.hpp>
#include <base/box.hpp>

namespace compiler::backend_llvm {
	struct ModuleImpl;
}

namespace base::extend {
	/**
	 * @brief Custom Box/MBox deleter for ModuleImpl.
	 * It is needed to avoid UB with delete on incomplete type.
	 */
	template<>
	struct BoxPtrDeleter<compiler::backend_llvm::ModuleImpl> {
		static void del(compiler::backend_llvm::ModuleImpl* ptr);
	};
}

namespace compiler::backend_llvm {

	/**
	 * @brief Encapsulates a llvm module in a way
	 * that does not require to include llvm headers.
	 */
	struct Module {
	private:
		// this is done this way, to avoid including llvm headers here:
		Box<ModuleImpl> impl;

	public:
		Module(std::string_view module_id);

		Module(Box<ModuleImpl> impl): impl(std::move(impl)) {}

		void addFunctionToModule(CRef<lir::Function> lir_function);

		void debugPrint() const;

		[[nodiscard]]
		bool verify() const;

		~Module();
	};

	/**
	 * @brief Converts single lir function into a llvm module
	 * containing only this function.
	 * @note This function is a temporary entry point for the llvm backend.
	 */
	Module lirFunctionToModule(CRef<lir::Function>);
}
