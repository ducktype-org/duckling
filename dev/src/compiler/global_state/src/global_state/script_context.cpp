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
