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
	}

	constexpr std::array<std::string_view, microInstrCount()> OPCODE_NAMES = {
#define HANDLE_MICRO_INSTR(opcode) #opcode,
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
	};

	/**
	 * @brief For microinstruction name, returns corresponding MicroOpcode.
	 */
	constexpr vm::low::MicroOpcode getOpcode(std::string_view func_name) {
		for (auto [opcode, name]: std::views::enumerate(vm::low::OPCODE_NAMES))
			if (func_name == name) return static_cast<vm::low::MicroOpcode>(opcode);
		CORE_PANIC("Function name does not correspond to any MicroOpcode", func_name);
	}

	constexpr usize nonExecutableMicroInstrCount() {
		usize count = 0;
#define HANDLE_MICRO_INSTR(opcode) \
	if constexpr ((std::string_view(#opcode).starts_with("ext_"))) { ++count; }
#include "micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
		return count;
	}

	namespace internal {
		/**
		 * @brief Constructs an array of non-executable opcodes (like ext_*).
		 * Used to create NON_EXEC_OPCODES.
		 */
		constexpr std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()>
			constructNonExecOpcodeArray() {
			auto non_executable_opcodes
				= vm::low::OPCODE_NAMES | std::views::enumerate | std::views::filter([](auto pair) {
					  return std::get<1>(pair).starts_with("ext_");
				  })
			    | std::views::transform([](auto pair) {
					  return static_cast<vm::low::MicroOpcode>(std::get<0>(pair));
				  });

			std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()> output{};

			std::ranges::copy(non_executable_opcodes, output.begin());

			return output;
		}
	}

	static constexpr std::array<vm::low::MicroOpcode, vm::low::nonExecutableMicroInstrCount()>
		NON_EXEC_OPCODES = internal::constructNonExecOpcodeArray();

	/**
	 * @brief Returns whether opcode is not executable like ext_*.
	 */
	constexpr bool isOpcodeNonExecutable(const vm::low::MicroOpcode& opcode) {
		return std::ranges::find(NON_EXEC_OPCODES, opcode) != NON_EXEC_OPCODES.end();
	}
}
