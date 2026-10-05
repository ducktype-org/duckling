// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <driver/task/task.hpp>

#include <base/types/ok_bad.hpp>

#include <artifacts/artifacts.hpp>

#include <vm/bytecode/bytecode.hpp>

#include <vector>

namespace compiler::driver {

	/**
	 * @brief Removes duplicate functions, external C functions, globals, and types from the
	 * collection, keeping only the first occurrence of each name.
	 * @note Does not perform any assertions.
	 */
	void deduplicateCodeCollection(vm::code::CodeCollection& code);

	/**
	 * @brief Links all per-module DVM .dbc files into a single merged package .dbc,
	 * and merges per-module debug info files if provided.
	 *
	 * This is the DVM analogue of linking .o object files for LLVM.
	 */
	base::OkBad linkDVMPackage(
		const std::vector<artifacts::FileArtifact>& objects,
		const std::vector<artifacts::FileArtifact>& debug_info_artifacts,
		const DVMLinkingOptions&                    dvm_linking_options,
		artifacts::FileArtifact&                    output_file
	);
}
