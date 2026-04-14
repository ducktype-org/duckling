/**
 * @file opcodes.hpp
 * @brief Defines enum for all opcodes in the VM.
 */
#pragma once

#include <vm/bytecode/instructions.hpp>

/**
 * Opcodes names conventions:
 *
 * Name is: name_[first arg description]_[optional second arg description]
 * Each name is:
 * imm      - immediate value
 * l[size]  - position of primitive local with given size
 * lptr     - position of local pointer
 * func     - function id
 * r[nr]    - primitive register with number [nr]
 * rprt[nr] - pointer register with number [nr]
 * type     - type name
 * label    - label name
 *
 * Most two argument operation store result in first argument
 *
 * Opcodes not following this convention have additional description
 */

namespace vm::low {
	enum class MicroOpcode : u64 {
#define HANDLE_MICRO_INSTR(opcode) opcode,
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
	};

	constexpr usize microInstrCount() {
		usize count = 0;
#define HANDLE_MICRO_INSTR(instr) ++count;
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
		return count;
	};

	constexpr std::array<std::string_view, microInstrCount()> OPCODE_NAMES = {
#define HANDLE_MICRO_INSTR(opcode) #opcode,
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
	};

	constexpr usize nonExecutableMicroInstrCount() {
		usize count = 0;
#define HANDLE_MICRO_INSTR(opcode) if constexpr ((std::string_view(#opcode).starts_with("ext_"))) { ++count; }
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
		return count;
	};
}
