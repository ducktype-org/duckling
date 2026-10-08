// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/types/ints.hpp>

namespace vm::fast {
	class ThreadRuntimeState;
	class FastVMThread;
	struct Frame;

	namespace exec {
		struct Instruction;
	}
}

#define INSTRFUN_ARGS_BASE_TC                                                                 \
	[[maybe_unused]] vm::fast::ThreadRuntimeState &state, [[maybe_unused]] byte *local_stack, \
		[[maybe_unused]] Frame *frame, [[maybe_unused]] FastVMThread &thread
#define INSTRFUN_ARGS_BASE_SC                                                                  \
	[[maybe_unused]] vm::fast::ThreadRuntimeState &state, [[maybe_unused]] byte *&local_stack, \
		[[maybe_unused]] Frame *&frame, [[maybe_unused]] FastVMThread &thread

#ifdef USE_TAIL_CALLS
	#define INSTRFUN_ARGS_BASE INSTRFUN_ARGS_BASE_TC
#else
	#define INSTRFUN_ARGS_BASE INSTRFUN_ARGS_BASE_SC
#endif


namespace vm::fast {
	using DispatcherFunction
		= void (*)(INSTRFUN_ARGS_BASE, [[maybe_unused]] const exec::Instruction& instr);
}
