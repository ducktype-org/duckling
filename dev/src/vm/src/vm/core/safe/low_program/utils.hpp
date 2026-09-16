#pragma once

#include "micro_instruction_args.hpp"
#include "opcodes.hpp"

#include <tuple>

/**
 * Helper types, type aliases and constants used for lowering into micro bytecode.
 * There is no actual runnable code here, no actual runtime data, the structs
 * serve as holders for type aliases.
 * This file is meant to hold most of the ugly uBC related X-macros in one place,
 * allowing the rest of the code to use C++ metaprogramming instead of macros.
 */

/// Tags for use as template parameters.
namespace vm::low::instruction_tags {
#define HANDLE_MICRO_INSTR_0ARGS(INSTR)                           \
	struct Op_##INSTR final {                                     \
		using ArgTypes                      = std::tuple<>;       \
		static constexpr MicroOpcode OPCODE = MicroOpcode::INSTR; \
	};

#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0)                     \
	struct Op_##INSTR final {                                     \
		using ArgTypes                      = std::tuple<ARG0>;   \
		static constexpr MicroOpcode OPCODE = MicroOpcode::INSTR; \
	};

#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1)                   \
	struct Op_##INSTR final {                                         \
		using ArgTypes                      = std::tuple<ARG0, ARG1>; \
		static constexpr MicroOpcode OPCODE = MicroOpcode::INSTR;     \
	};

#include "micro_instruction_definitions.def.hpp"
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS

	namespace detail {
		template<typename T>
		inline constexpr bool IS_MICRO_TAG = false;

		template<typename T, usize N>
		inline constexpr bool IS_ARG_LABEL = false;

#define HANDLE_MICRO_INSTR_0ARGS(INSTR) \
	template<>                          \
	inline constexpr bool IS_MICRO_TAG<Op_##INSTR> = true;

#define HANDLE_MICRO_INSTR_1ARGS(INSTR, ARG0)              \
	template<>                                             \
	inline constexpr bool IS_MICRO_TAG<Op_##INSTR> = true; \
	template<>                                             \
	inline constexpr bool IS_ARG_LABEL<Op_##INSTR, 0> = std::same_as<ARG0, opargs::Label>;

#define HANDLE_MICRO_INSTR_2ARGS(INSTR, ARG0, ARG1)                                        \
	template<>                                                                             \
	inline constexpr bool IS_MICRO_TAG<Op_##INSTR> = true;                                 \
	template<>                                                                             \
	inline constexpr bool IS_ARG_LABEL<Op_##INSTR, 0> = std::same_as<ARG0, opargs::Label>; \
	template<>                                                                             \
	inline constexpr bool IS_ARG_LABEL<Op_##INSTR, 1> = std::same_as<ARG1, opargs::Label>;

#include "micro_instruction_definitions.def.hpp"
#undef HANDLE_MICRO_INSTR_0ARGS
#undef HANDLE_MICRO_INSTR_1ARGS
#undef HANDLE_MICRO_INSTR_2ARGS
	}

	/// Concept for detecting vm::low::instruction_tags::Op_*
	template<typename T>
	concept IsMicroInstructionTag = detail::IS_MICRO_TAG<T>;

	/**
	 * @brief Array used for checking if a given argument of a given instruction is a label.
	 * This is used for inspecting lowered untyped vm::MicroInstruction structs
	 * when linking labels.
	 */
	inline constexpr auto IS_ARGUMENT_LABEL = std::to_array<std::array<bool, 2>>({
#define HANDLE_MICRO_INSTR(INSTR) \
	{ detail::IS_ARG_LABEL<Op_##INSTR, 0>, detail::IS_ARG_LABEL<Op_##INSTR, 1> },
#include "micro_instruction_definitions.def.hpp"
#undef HANDLE_MICRO_INSTR
	});
}
