#pragma once

#include "../backend_module_data.hpp"

#include <query_framework/context_fd.hpp>

#include <vm/bytecode/bytecode.hpp>

namespace compiler::driver {
	vm::code::CodeCollection compileLIRModuleToDVM(
		query::Context& ctx, const LIRModuleData& lir_module
	);
}
