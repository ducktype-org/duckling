/**
 * @file opcodes.hpp
 * @brief Defines enum for all opcodes in the VM.
 */
#pragma once

#include "base/exceptions.hpp"
#include "base/variant.hpp"
#include "vm/program/instructions.hpp"
#include <base/ints.hpp>
#include <type_traits>
#include <unordered_map>

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
	enum class OpcodeFix8 : u16 {
#define HANDLE_OPCODE(opcode) opcode,
#include <vm/program/opcodes_list.hpp>
#undef HANDLE_OPCODE
		Comment
	};

	template<class Instr>
	struct InstrToOpcodeFix8;

#define HANDLE_OPCODE(opcode)                                         \
	template<>                                                        \
	struct InstrToOpcodeFix8<VM_INSTR_FROM_NAME(opcode)> {            \
		static constexpr OpcodeFix8 OPCODE_FIX8 = OpcodeFix8::opcode; \
	};

	template<>
	struct InstrToOpcodeFix8<program::instructions::Comment> {
		static constexpr OpcodeFix8 OPCODE_FIX8 = OpcodeFix8::Comment;
	};

#include <vm/program/opcodes_list.hpp>
#undef HANDLE_OPCODE

	u16 fix8FromInstr(const program::Instruction& instruction) {
		return u16(
			VISIT(instruction,
		          var,
		          return low::InstrToOpcodeFix8<std::remove_cvref_t<decltype(var)>>::OPCODE_FIX8;)
		);
	}
}
