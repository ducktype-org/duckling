#pragma once

#include <base/types/ints.hpp>

namespace compiler::backend_vm {
	/**
	 * @brief Snapshot of lowered entities (types/functions/extra functions).
	 */
	struct LoweredEntitiesSnapshot {
		usize lowered_type_count            = 0;
		usize lowered_global_count          = 0;
		usize lowered_function_count        = 0;
		usize extra_bytecode_function_count = 0;
	};
}
