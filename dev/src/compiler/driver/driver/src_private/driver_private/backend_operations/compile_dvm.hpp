#pragma once

#include "../lir_module_data.hpp"

#include <query_framework/context/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::driver {
	/**
	 * @brief Compiles the LIRModuleData to DVM CodeCollection.
	 */
	vm::code::CodeCollection compileLIRModuleToDVM(const LIRModuleData& lir_module);
}
