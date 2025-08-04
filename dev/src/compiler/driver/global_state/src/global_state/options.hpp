#pragma once

#include <base/ref.hpp>

namespace global_state {
	/**
	 * Some debug options, that can be freely changed during compiler operations.
	 * This should be only used via getDynamicDebugOptions function.
	 * @TODO: #1058 this is the same category as lexer-cerr/logger-cerr options, we should add some
	 * consistent abstraction for that.
	 */
	struct DynamicDebugOptions final {
		// When compiling llvm files also dump their IR representation
		bool llvm_dump_ir;

		// When compiling llvm files also dump assembly representation
		bool llvm_dump_asm;
	};

	/**
	 * @note This can be used for modification
	 */
	Ref<DynamicDebugOptions> getDynamicDebugOptions();


}
