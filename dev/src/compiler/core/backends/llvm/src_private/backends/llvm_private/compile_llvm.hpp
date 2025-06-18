#pragma once

#include <backends/llvm/llvm_backend.hpp>

namespace compiler::backend_llvm {
	/**
	 * @brief Compiles the module to an object file or assembly.
	 *
	 * @param module_impl The module to compile.
	 * @param output_file Path where the output file will be saved.
	 * @param output_type Type of the output file (object or assembly).
	 */
	void compileModuleToObject(
		Ref<ModuleImpl>              module_impl,
		const std::filesystem::path& output_file,
		CompilationOutputType        output_type
	);
}
