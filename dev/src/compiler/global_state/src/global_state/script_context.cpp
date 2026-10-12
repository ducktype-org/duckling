// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "script_context.hpp"

#include <base/except/exceptions.hpp>

namespace global_state {
	namespace {
		base::Optional<ScriptContext> script_context;
	}

	const ScriptContext& getScriptContext() {
		CORE_ASSERT(
			script_context.has_value(),
			"Script context not initialized. Call setScriptContext() during initialization."
		);
		return script_context.value();
	}

	namespace setters {
		void setScriptContext(const fs::File& script_file) { script_context.emplace(script_file); }
	}
}
