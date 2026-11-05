// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <backends/llvm/llvm_backend.hpp>

namespace compiler::backend_llvm {
	/**
	 * @brief Compiles the module to an object file or assembly.
	 *
	 * @param module_impl The module to compile.
	 * @param output_file Path where the output file will be saved.
	 * @param output_type Type of the output file (object or assembly).
	 * @return Nothing on success, or a message describing why the file could not be written.
	 */
	std::expected<void, std::string> compileModuleToObject(
		Ref<ModuleImpl>              module_impl,
		const std::filesystem::path& output_file,
		CompilationOutputType        output_type
	);
}
