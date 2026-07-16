// @note This file is relevant only in tailcall mode
#include "executable.hpp"

#include <vm/core/config.hpp>
#include <vm/core/musttail.hpp>

#ifdef USE_TAIL_CALLS

	#include <vm/core/fast/eval/evaluator.hpp>

using namespace vm::fast;


// Creates a dispatcher for an instruction.
// Dispatcher's role is to call the implementation of the instruction as well
// as next instruction's dispatcher (if instruction is not `exit`).
	#define HANDLE_INSTR(NAME)                                                                 \
		constexpr void dispatcher_##NAME(                                                      \
			INSTRFUN_ARGS_BASE, [[maybe_unused]] const vm::fast::exec::Instruction& instr      \
		) {                                                                                    \
			FastExecutor::Instr_##NAME(state, local_stack, frame, thread, instr.instr_##NAME); \
			if constexpr (std::string_view(#NAME) != "exit") {                                 \
				MUST_TAIL return frame->ip->id(state, local_stack, frame, thread, *frame->ip); \
			}                                                                                  \
		}
	#include <vm/core/fast/program/instructions/instruction_definitions.hpp>
	#undef HANDLE_INSTR

// NOLINTNEXTLINE(modernize-concat-nested-namespaces) inner `maker` namespace comes from the include
namespace vm::fast::exec {
	#define ARG_NAMESPACE           vm::fast::exec::arg::
	#define ID_TYPE()               vm::fast::DispatcherFunction
	#define MAKE_ID_FROM_NAME(NAME) dispatcher_##NAME
	#define MAKE_MAKERS_JUST_IMPL
	#include "instr_structures.hpp"
}


#endif
