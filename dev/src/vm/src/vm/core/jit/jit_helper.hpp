// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "jit_compiler.hpp"

#include <vm/core/safe/low_program/instruction.hpp>

#include <string_view>

namespace vm::jit::helpers {
	void trampoline(OPFUN_REF_ARGS);

	/**
	 * @brief Prod-side handling of a failed JIT compilation.
	 * @details Logs an error and permanently disables compilation attempts for the entrypoint
	 * (dev builds assert on the failure before this is ever reached). The caller then falls
	 * back to interpreting the original instruction(s).
	 */
	void disableEntrypointAfterFailure(
		JitFuncData& data, usize cfg_offset, std::string_view func_name, std::string_view error
	);
}
