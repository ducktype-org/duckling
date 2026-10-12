// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file opcodes.hpp
 * @brief Defines enum for all opcodes in the VM.
 */
#pragma once

#include <vm/bytecode/instructions.hpp>

/**
 * Instruction naming convention (high and micro instructions alike):
 *
 *   baseName[_operand1[_operand2[_operand3]]]
 *
 * `baseName` is camelCase (`add`, `cmpEq`, `bitAnd`, `virtualCall`). It is followed by the short
 * name of each operand, in order: `OP_SHORT` in `vm/bytecode/opcode_args.hpp` for high
 * instructions, `ARG_SHORT` in `micro_instruction_args.hpp` for micro ones. For example:
 * imm          - immediate value
 * p8..p64      - place of a primitive of the given size
 * pptr, pcptr  - place of a VM pointer / of a C pointer
 * pany, popq   - place of any type / of an opaque value
 * pste, pfst   - place of a structure / of a fixed-size table
 * pvnt         - place of a variant
 * bany, bste,
 * bfst, bvnt   - (micro only) block place of any type / structure / fixed-size table / variant
 * off          - (micro only) local stack offset computed by the lowering
 * type, field  - type name / field name
 * func, method - function name / method name
 * label        - label name
 *
 * `vm_instruction_naming_test` enforces it. Its only exceptions are the `label` pseudo-instruction
 * and `ptrParts_p64_p64_pptr`, whose micro form reads its third operand from an `ext_pptr`.
 *
 * Most two argument operations store the result in the first argument.
 */

namespace vm::low {
	enum class MicroOpcode : u64 {
#define HANDLE_MICRO_INSTR(opcode) opcode,
#include "micro_instruction_definitions.def.hpp"
#undef HANDLE_MICRO_INSTR
	};

	constexpr usize microInstrCount() {
		usize count = 0;
#define HANDLE_MICRO_INSTR(instr) ++count;
#include "micro_instruction_definitions.def.hpp"
#undef HANDLE_MICRO_INSTR
		return count;
	}

	constexpr std::array<std::string_view, microInstrCount()> OPCODE_NAMES = {
#define HANDLE_MICRO_INSTR(opcode) #opcode,
#include "micro_instruction_definitions.def.hpp"
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
#include "micro_instruction_definitions.def.hpp"
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

	template<vm::low::MicroOpcode opcode>
	constexpr bool IS_OPCODE_RETURNING
		= opcode == MicroOpcode::ret || opcode == MicroOpcode::retTailcall_func;
}
