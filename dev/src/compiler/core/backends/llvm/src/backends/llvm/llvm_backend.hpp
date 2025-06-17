#pragma once

#include <lir/lir_structure/function_forward.hpp>
#include <query_framework/context_fd.hpp>

#include <base/box.hpp>
#include <base/ok_bad.hpp>
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

		Module(Module&&)      = default;
		Module(const Module&) = delete;


		/**
		 * @brief Creates a llvm module from llvm IR code given as a text input.
		 * Panics if the code is invalid.
		 *
		 * @return Module created by parsing the given IR code.
		 */
		static Module fromIRCode(std::string_view llvm_ir_code);

		Module(Box<ModuleImpl> impl): impl(std::move(impl)) {}

		void addFunctionToModule(query::Context&, CRef<lir::Function> lir_function);

		/**
		 * @brief Adds a global variable declaration to the module.
		 *
		 * This function declares a global variable in the LLVM module and initializes it to 0 or
		 * null. Note: This does not add a constructor for the global variable.
		 *
		 * @param lir_global The global variable to be added to the module.
		 */
		void addGlobalToModule(const lir::LirGlobal& lir_global);

		/**
		 * @brief Adds a function to the LLVM module's list of global constructors.
		 *
		 * This function registers a function as a global constructor in the LLVM module.
		 *
		 * @param ctx The query context.
		 * @param lir_function The function to be added as a global constructor.
		 */
		void addFunctionToModuleCtors(query::Context& ctx, CRef<lir::Function> lir_function);

		/**
		 * @brief Adds a function to the LLVM module's list of global destructors.
		 *
		 * This function registers a function as a global destructor in the LLVM module.
		 *
		 * @param ctx The query context.
		 * @param lir_function The function to be added as a global destructor.
		 */
		void addFunctionToModuleDtors(query::Context& ctx, CRef<lir::Function> lir_function);

		void debugPrint() const;

		/**
		 * @brief Dumps the LLVM IR to a file.
		 *
		 * @param output_file Path where the output file will be saved.
		 */
		void debugDumpToFile(base::StrID output_file) const;

		[[nodiscard]]
		base::OkBad verify() const;

		/**
		 * @brief Compile the module to binary object file or assembly file.
		 *
		 * @param output_file Path where the output file will be saved.
		 * @param output_type Type of the output file.
		 */
		void compile(base::StrID output_file, CompilationOutputType output_type);

		/**
		 * Returns the number of functions in the module.
		 * It is used for testing purposes.
		 * @param including_prototypes If false, doesn't count prototypes (function without
		 * definitions) in the result.
		 */
		[[nodiscard]]
		u64 getFunctionCount(bool including_prototypes = true) const;

		~Module();
	};
}
