#pragma once

#include "opcodes.hpp"

#include <vm/bytecode/opcode_args.hpp>

#include <tuple>

namespace vm::low::instruction_tags {
#define HANDLE_MICRO_INSTR_0ARGS(INSTR)                           \
	struct Op_##INSTR {                                           \
		using ArgTypes                      = std::tuple<>;       \
		static constexpr MicroOpcode OPCODE = MicroOpcode::INSTR; \
	};

#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0)                     \
	struct Op_##INSTR {                                           \
		using ArgTypes                      = std::tuple<ARG0>;   \
		static constexpr MicroOpcode OPCODE = MicroOpcode::INSTR; \
	};

#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1)                   \
	struct Op_##INSTR {                                               \
		using ArgTypes                      = std::tuple<ARG0, ARG1>; \
		static constexpr MicroOpcode OPCODE = MicroOpcode::INSTR;     \
	};

#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

	namespace {
		template<typename T>
		constexpr bool IS_MICRO_TAG = false;

		template<typename T, usize N>
		constexpr bool IS_ARG_LABEL = false;

#define HANDLE_MICRO_INSTR_0ARGS(INSTR) \
	template<>                          \
	constexpr bool IS_MICRO_TAG<Op_##INSTR> = true;

#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0)       \
	template<>                                      \
	constexpr bool IS_MICRO_TAG<Op_##INSTR> = true; \
	template<>                                      \
	constexpr bool IS_ARG_LABEL<Op_##INSTR, 0> = std::same_as<ARG0, opargs::Label>;

#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1)                                 \
	template<>                                                                      \
	constexpr bool IS_MICRO_TAG<Op_##INSTR> = true;                                 \
	template<>                                                                      \
	constexpr bool IS_ARG_LABEL<Op_##INSTR, 0> = std::same_as<ARG0, opargs::Label>; \
	template<>                                                                      \
	constexpr bool IS_ARG_LABEL<Op_##INSTR, 1> = std::same_as<ARG1, opargs::Label>;

#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS
	}

	template<typename T>
	concept IsMicroInstructionTag = IS_MICRO_TAG<T>;

	constexpr auto IS_ARGUMENT_LABEL = std::to_array<std::array<bool, 2>>({
#define HANDLE_MICRO_INSTR(INSTR) { IS_ARG_LABEL<Op_##INSTR, 0>, IS_ARG_LABEL<Op_##INSTR, 1> },
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
	});
}
