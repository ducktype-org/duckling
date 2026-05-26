#pragma once

#include "module_impl_fd.hpp"

#include <lir/lir_structure/function_forward.hpp>

#include <base/pointers/box.hpp>
#include <base/types/ok_bad.hpp>

#include <query_framework/context/context_fd.hpp>
#include <string_id/string_id.hpp>

#include <filesystem>

namespace compiler::backend_llvm {
	enum class CompilationOutputType : std::uint8_t { Object, Assembly };

	/**
	 * @brief Encapsulates a llvm module in a way
	 * that does not require to include llvm headers.
	 */
	struct Module final {
	private:
		// this is done this way, to avoid including llvm headers here:
		Box<ModuleImpl> impl;

	public:
		Module(base::StrID module_id);

		Module(Module&&)      = default;
		Module(const Module&) = delete;


		/**
		 * @brief Creates an LLVM module from LLVM IR code given as a text input.
		 * Panics if the code is invalid.
		 *
		 * @return Module created by parsing the given IR code.
		 */
		static Module fromIRCode(std::string_view llvm_ir_code);

		/**
		 * @brief Creates an LLVM module from LLVM bitcode given as char span.
		 * Panics if the code is invalid.
		 *
		 * @return Module created by parsing the given bitcode.
		 */
		static Module fromLLVMBC(std::span<unsigned char> llvm_bc_data);

		Module(Box<ModuleImpl> impl): impl(std::move(impl)) {}

		void addFunctionToModule(query::Context&, CRef<lir::Function> lir_function);

		/**
		 * @brief Adds a global variable declaration to the module.
		 *
		 * This function declares a global variable in the LLVM.
		 * Note: If the global is a constant, it will be initialized with the provided value.
		 * If the global is a variable, it will be zero-initialized by default, and the provided value will be ignored.
		 * PR: improve the above!
		 *
		 * @param lir_global The global variable to be added to the module.
		 */
		void addGlobalToModule(const lir::LIRGlobal& lir_global);

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
		void dumpLLVMToFile(base::StrID output_file) const;

		/**
		 * @brief Dumps the LLVM IR to string.
		 */
		[[nodiscard]] std::string dumpLLVMToString() const;

		[[nodiscard]]
		base::OkBad verify() const;

		/**
		 * @brief Compile the module to binary object file or assembly file.
		 *
		 * @param output_file Path where the output file will be saved.
		 * @param output_type Type of the output file.
		 */
		void compile(const std::filesystem::path& output_file, CompilationOutputType output_type);

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
