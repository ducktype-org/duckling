#pragma once

#include <vm/bytecode/opcode_args.hpp>

#include <tuple>

namespace vm::low::instruction_tags {
#define HANDLE_MICRO_INSTR_0ARGS(INSTR) \
	struct Op_##INSTR {                 \
		using ArgTypes = std::tuple<>;  \
	};
#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0) \
	struct Op_##INSTR {                       \
		using ArgTypes = std::tuple<ARG0>;    \
	};
#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1) \
	struct Op_##INSTR {                             \
		using ArgTypes = std::tuple<ARG0, ARG1>;    \
	};
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

	namespace {
		template<typename T>
		constexpr bool IS_MICRO_TAG = false;

#define HANDLE_MICRO_INSTR_0ARGS(INSTR) \
	template<>                          \
	constexpr bool IS_MICRO_TAG<Op_##INSTR> = true;
#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0) \
	template<>                                \
	constexpr bool IS_MICRO_TAG<Op_##INSTR> = true;
#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1) \
	template<>                                      \
	constexpr bool IS_MICRO_TAG<Op_##INSTR> = true;
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS
	}

	template<typename T>
	concept IsMicroInstructionTag = IS_MICRO_TAG<T>;
}
