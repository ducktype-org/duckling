#include "module_flags.hpp"

namespace compiler::driver {
	constinit bool llvm_dump_ir                   = false;
	constinit bool llvm_dump_asm                  = false;
	constinit bool enable_incremental_compilation = false;
}
