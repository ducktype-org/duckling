#pragma once

#include <vm/core/config.hpp>
#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/runtime.hpp>

#ifdef USE_TAIL_CALLS
	#define INSTRFUN_ARGS_BASE                                                                    \
		[[maybe_unused]] vm::fast::ThreadRuntimeState &state, [[maybe_unused]] byte *local_stack, \
			[[maybe_unused]] Frame *frame
#else
	#define INSTRFUN_ARGS_BASE                                                                     \
		[[maybe_unused]] vm::fast::ThreadRuntimeState &state, [[maybe_unused]] byte *&local_stack, \
			[[maybe_unused]] Frame *&frame
#endif
#define INSTRFUN_ARGS(name) \
	INSTRFUN_ARGS_BASE, [[maybe_unused]] const vm::fast::exec::instr_structs::name& instr

namespace vm::fast {
	class ThreadRuntimeState;

	class FastExecutor {
	public:
#define HANDLE_INSTR(NAME) static constexpr void Instr_##NAME(INSTRFUN_ARGS(NAME));
#include <vm/core/fast/program/instructions/instruction_definitions.hpp>
#undef HANDLE_INSTR

		static constexpr int eval(INSTRFUN_ARGS_BASE);
	};

}
