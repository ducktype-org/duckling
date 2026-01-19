#pragma once

#include <base/collections/optional.hpp>

namespace compiler::options {
	// @note: a lot of code in this file is left as hypothetical comments
	// as it is unused for now, but sets a vision for the code structure
	// in the future.
	// I'm not 100% sure if this place is the best place for this code,
	// as in the end those options should be accessible (via global_state module of other things)
	// in the core-compiler, and we don't want to have a dependency on the driver module there.

	/**
	 * Options used for actual compilation of the source code.
	 */
	struct BackendOptions final {
		struct DVMBackend {};

		struct LLVMBackend {
			enum class LLVMOptimizationLevel { O0, O1, O2, O3, Os, Oz };

			LLVMOptimizationLevel llvm_optimization_level{ LLVMOptimizationLevel::O0 };
		};

		// /**
		//  * If empty, then DVM backend is not available.
		//  */
		// base::Optional<VMBackend> dvm_backend;

		/**
		 * If empty, then LLVM backend is not available.
		 */
		base::Optional<LLVMBackend> llvm_backend;
	};
};
