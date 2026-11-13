// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "dispatcher.hpp"

#include <vm/core/config.hpp>
#include <vm/core/fast/program/instructions/executable.hpp>
#include <vm/core/fast/runtime.hpp>

#define INSTRFUN_ARGS(name) \
	INSTRFUN_ARGS_BASE_SC, [[maybe_unused]] const vm::fast::exec::instr_structs::name& instr

namespace vm::fast {
	class FastExecutor {
	public:
#define HANDLE_INSTR(NAME) static void Instr_##NAME(INSTRFUN_ARGS(NAME)) noexcept;
#include <vm/core/fast/program/instructions/instruction_definitions.def.hpp>
#undef HANDLE_INSTR

		/**
		 * @brief Runs from `frame->ip` until `exit`. Defined next to the handlers so the switch
		 * loop inlines them. In tail-call mode it calls the first dispatcher, which cannot be
		 * `noexcept` (see `executable.cpp`).
		 */
		static void eval(INSTRFUN_ARGS_BASE) IF_NOT_TC(noexcept);
	};
}
