// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <filesystem/file.hpp>

namespace global_state {

	/**
	 * Global state for script compilation mode.
	 * Stores the script file being compiled/executed.
	 */
	struct ScriptContext {
		fs::File script_file;
	};

	/**
	 * Returns the current script context, if one is active.
	 */
	const ScriptContext& getScriptContext();

	namespace setters {
		/**
		 * Sets the script file for script compilation mode.
		 */
		void setScriptContext(const fs::File& script_file);
	}
}
