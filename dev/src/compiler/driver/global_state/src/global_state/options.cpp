
#include "options.hpp"

namespace global_state {

	Ref<DynamicDebugOptions> getDynamicDebugOptions() {
		static DynamicDebugOptions options{
			.llvm_dump_ir  = false,
			.llvm_dump_asm = false,
		};
		return &options;
	}

}
