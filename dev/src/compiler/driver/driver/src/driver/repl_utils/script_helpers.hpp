// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <driver_private/lir_unit_with_name.hpp>

#include <filesystem/file.hpp>

namespace compiler::repl {
	/**
	 * @brief Build a stable synthetic module ID for a script file.
	 *
	 * The id is derived from the script-file path hash and is used to name generated
	 * artifacts and keep naming deterministic across runs.
	 */
	base::StrID getScriptModuleID(const fs::File& script_file);

	/**
	 * @brief Append all functions and globals from one LIR module chunk into another.
	 *
	 * Script compilation lowers each statement into a chunk and then merges those
	 * chunks into a single synthetic module. This helper performs that merge step.
	 */
	void appendScriptLIRModuleData(
		driver::LIRUnitWithBackendName& merged, const driver::LIRUnitWithBackendName& chunk
	);

}  // namespace compiler::repl
