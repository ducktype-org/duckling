#pragma once

#include <frontend/module_tree/module_id.hpp>

#include <base/collections/optional.hpp>

#include <filesystem/file.hpp>

namespace global_state {

	/**
	 * @brief Global state for `duckc run` / `compile_script` (ScriptMode).
	 *
	 * Set during driver initialization. The driver reads this when lowering the script to LIR.
	 *
	 * Script runs always set `script_module_id` after locating the `.ds` file in the package
	 * module tree (standalone scripts are represented as script-only packages).
	 */
	struct ScriptContext {
		/** The `.ds` file passed on the command line. */
		fs::File script_file;
		/**
		 * Script node in the package module tree (`isScriptModule()`).
		 * Drives `buildScriptModuleFromTreeNode` for all script runs.
		 */
		compiler::frontend::ModuleID script_module_id;
	};

	/**
	 * Returns the current script context, if one is active.
	 */
	const ScriptContext& getScriptContext();

	namespace setters {
		/**
		 * Sets script context for script compilation mode.
		 */
		void setScriptContext(
			const fs::File& script_file, compiler::frontend::ModuleID script_module_id
		);
	}
}
