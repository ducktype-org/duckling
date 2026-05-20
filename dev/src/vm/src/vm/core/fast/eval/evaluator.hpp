#pragma once

#include <vm/core/config.hpp>
#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/runtime.hpp>

#ifdef USE_TAIL_CALLS
	#define INSTRFUN_ARGS_BASE                                                                    \
		[[maybe_unused]] vm::fast::ThreadRuntimeState &state, [[maybe_unused]] byte *local_stack, \
			[[maybe_unused]] Frame *frame, [[maybe_unused]] FastVMThread &thread
#else
	#define INSTRFUN_ARGS_BASE                                                                     \
		[[maybe_unused]] vm::fast::ThreadRuntimeState &state, [[maybe_unused]] byte *&local_stack, \
			[[maybe_unused]] Frame *&frame, [[maybe_unused]] FastVMThread &thread
#endif
#define INSTRFUN_ARGS(name) \
	INSTRFUN_ARGS_BASE, [[maybe_unused]] const vm::fast::exec::instr_structs::name& instr

namespace vm::fast {
	class ThreadRuntimeState;
	class FastVMThread;

	class FastExecutor {
	public:
#define HANDLE_INSTR(NAME) static void Instr_##NAME(INSTRFUN_ARGS(NAME));
#include <vm/core/fast/program/instructions/instruction_definitions.hpp>
#undef HANDLE_INSTR

		constexpr static int eval(INSTRFUN_ARGS_BASE) {
			bool run = true;
			while (run) {
				switch (state.currentInstruction().id) {
#define HANDLE_INSTR(NAME)                                                            \
	case vm::fast::InstrID::NAME: {                                                   \
		if constexpr (std::string_view(#NAME) != "exit") {                            \
			Instr_##NAME(state, local_stack, frame, thread, frame->ip->instr_##NAME); \
		} else {                                                                      \
			run = false;                                                              \
		}                                                                             \
		break;                                                                        \
	}
#include <vm/core/fast/program/instructions/instruction_definitions.hpp>
#undef HANDLE_INSTR
				default:
					return 1;
					break;
				}
			}
			return 0;
		}
	};

}
