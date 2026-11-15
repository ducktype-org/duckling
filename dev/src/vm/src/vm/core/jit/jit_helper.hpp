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
	/**
	 * @brief Runs one call instruction of JIT-compiled code in the interpreter.
	 * @details `instr` must point at the call in the function's pristine bytecode
	 * (`LowFuncData::orig_bc`). When a runtime error escapes the call, the caller frame gets its
	 * return address back (see `restoreReturnAddress`), so a stack trace shows its call site.
	 */
	void trampoline(OPFUN_REF_ARGS);

	/**
	 * @brief Points the return address of `caller` at `resume`, an instruction of its function's
	 * pristine bytecode, translated to the bytecode the interpreter runs.
	 * @details Leaves the frame as it is when `resume` is not in that pristine bytecode.
	 */
	void restoreReturnAddress(Frame& caller, const vm::MicroInstruction* resume);

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
