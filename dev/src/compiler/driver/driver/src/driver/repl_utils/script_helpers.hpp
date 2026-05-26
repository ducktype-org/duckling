#pragma once

#include <driver_private/lir_module_data.hpp>

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
	void appendScriptLIRModuleData(driver::LIRUnitWithBackendName& merged, const driver::LIRUnitWithBackendName& chunk);

}  // namespace compiler::repl
