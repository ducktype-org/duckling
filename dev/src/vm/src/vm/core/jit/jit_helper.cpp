// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "jit_helper.hpp"

#include <base/except/exceptions.hpp>

#include <logger/logger.hpp>

#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/safe_vmthread.hpp>

#include <array>
#include <limits>

namespace vm::jit::helpers {
	void trampoline(OPFUN_REF_ARGS) {
		// Some instructions consume following ext_* slots as extra arguments. Of the instructions
		// that can be trampolined (the non-jittable ones), only virtualCall does so, reading its
		// local stack distance from a following ext_imm.
		const usize slot_count
			= vm::getInstructionOpcode(*instr) == vm::low::MicroOpcode::virtualCall_pptr_method ? 2
		                                                                                        : 1;
		CORE_ASSERT(
			slot_count == 1 || vm::getInstructionOpcode(instr[1]) == vm::low::MicroOpcode::ext_imm,
			"virtualCall must be followed by an ext_imm slot"
		);

		std::array<vm::MicroInstruction, 3> buffer{};
		for (usize i = 0; i < slot_count; ++i) buffer.at(i) = instr[i];
		// The exit bounds interpretation to this one instruction: a called function returns into
		// it, which stops the interpreter instead of continuing with whatever follows in bytecode.
		buffer.at(slot_count) = vm::makeLowInstruction(vm::low::MicroOpcode::exit, 0, 0);
		instr += slot_count;

		// The call saves its return address in the caller frame, and that address points into
		// `buffer`. A runtime error unwinds past this function, so the address would then point
		// nowhere, and a stack trace could not find the call site.
		Frame* const caller = frame;
		try {
			runInterpreter(buffer.data(), local_stack, frame, thread);
		} catch (...) {
			restoreReturnAddress(*caller, instr);
			throw;
		}
	}

	void restoreReturnAddress(Frame& caller, const vm::MicroInstruction* resume) {
		// The JIT compiles the pristine copy of the bytecode, which has the same layout as the
		// bytecode the interpreter runs.
		const auto& func = *caller.current_function;
		const auto* orig = func.orig_bc.data();
		if (resume < orig || resume > orig + func.orig_bc.size()) return;
		caller.instr = func.bc.data() + (resume - orig);
	}

	void disableEntrypointAfterFailure(
		JitFuncData& data, usize cfg_offset, std::string_view func_name, std::string_view error
	) {
		CORE_USER_LOG(
			"JIT compilation failed for '",
			func_name,
			"' entrypoint at offset ",
			cfg_offset,
			": ",
			error,
			". Falling back to the interpreter for it from now on.\n"
		);
		// The caller keeps decrementing this, so compilation is retried once every 2^32
		// executions of this entrypoint. That is rare enough to treat as "disabled", and a
		// retry that fails again simply lands back here.
		data.until_compilation[cfg_offset] = std::numeric_limits<uint>::max();
	}
}
