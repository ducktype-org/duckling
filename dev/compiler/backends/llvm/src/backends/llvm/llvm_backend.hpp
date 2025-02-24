#pragma once

#include <lir/lir_structure/function_forward.hpp>
#include <base/box.hpp>
#include <base/optional.hpp>
#include <base/string_id.hpp>

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
	enum class CompilationOutputType : std::uint8_t { Object, Assembly };

	/**
	 * @brief Encapsulates a llvm module in a way
	 * that does not require to include llvm headers.
	 */
	struct Module {
	private:
		// this is done this way, to avoid including llvm headers here:
		Box<ModuleImpl> impl;

	public:
		Module(base::StrID module_id);

		Module(Box<ModuleImpl> impl): impl(std::move(impl)) {}

		void addFunctionToModule(CRef<lir::Function> lir_function);

		void debugPrint() const;

		/**
		 * @brief Dumps the LLVM IR to a file.
		 *
		 * @param output_file Path where the output file will be saved.
		 */
		void debugDumpToFile(base::StrID output_file) const;

		[[nodiscard]]
		bool verify() const;

		/**
		 * @brief Compile the module to binary object file or assembly file.
		 *
		 * @param output_file Path where the output file will be saved.
		 * @param output_type Type of the output file.
		 */
		void compile(base::StrID output_file, CompilationOutputType output_type);

		~Module();
	};

	/**
	 * @brief Converts single lir function into a llvm module
	 * containing only this function.
	 * @note This function is a temporary entry point for the llvm backend.
	 */
	Module lirFunctionToModule(CRef<lir::Function>);
}
