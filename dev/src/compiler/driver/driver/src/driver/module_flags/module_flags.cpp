#include "module_flags.hpp"

namespace compiler::driver {
	constinit DumpIROptions  dump_ir_options;
	
	constinit PrintIROptions print_ir_options;

	constinit bool           enable_incremental_compilation = false;
}
