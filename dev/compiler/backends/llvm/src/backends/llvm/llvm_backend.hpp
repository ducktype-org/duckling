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

	struct ModuleCompilationOptions {
		enum class OutputType : std::uint8_t { Object, Assembly };

		base::StrID                 object_file_path;
		OutputType                  output_type;
		base::Optional<base::StrID> llvm_ir_path;
	};

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

		void compile(const ModuleCompilationOptions& options);

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
