/**
 * @file opcodes.hpp
 * @brief Defines enum for all opcodes in the VM.
 */
#pragma once

#include <base/ints.hpp>
#include <map>

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


namespace vm {
	enum class OpcodeFix8 : u16 {
#define HANDLE_OPCODE(opcode) opcode,
#include "opcodes_list.hpp"
#undef HANDLE_OPCODE
	};
	const std::map<std::string, OpcodeFix8> STR_TO_OPCODE_FIX8{
#define HANDLE_OPCODE(opcode) { #opcode, OpcodeFix8::opcode },
#include "opcodes_list.hpp"
#undef HANDLE_OPCODE
	};
}
