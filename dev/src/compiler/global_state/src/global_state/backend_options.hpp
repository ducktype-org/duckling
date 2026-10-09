// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>

namespace global_state {

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

	/**
	 * Returns the current backend options for compilation.
	 */
	CRef<BackendOptions> getBackendOptions();

	namespace setters {
		/**
		 * Sets the backend options for compilation.
		 */
		void setBackendOptions(BackendOptions options);
	}
}
