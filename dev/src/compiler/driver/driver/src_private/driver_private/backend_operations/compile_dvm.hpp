#pragma once

#include "../lir_unit_with_name.hpp"

#include <debug_info/debug_info.hpp>

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::driver {

	/**
	 * @brief Result of compiling a LIR module to DVM bytecode,
	 * containing the bytecode and optionally the debug info if it was built.
	 */
	struct DVMModuleData {
		vm::code::CodeCollection              code;
		base::Optional<debug_info::DebugInfo> debug_info;
	};

	/**
	 * @brief Compiles the LIRUnitWithBackendName to DVM CodeCollection.
	 * @TODO: #2246 consider moving this logic to the DVM backend
	 */
	[[nodiscard]] DVMModuleData compileLIRModuleToDVM(
		CRef<LIRUnitWithBackendName> lir_module, query::Context& query_ctx, bool build_debug_info
	);
}
