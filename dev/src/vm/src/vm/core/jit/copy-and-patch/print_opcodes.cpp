/**
 * @file print_opcodes.cpp
 * @brief Print opcode metadata as json: a map of micro instruction names (including special
 * stencils) to opcode values, and a list of the non-jittable micro instruction names.
 * @details The goal here is to facilitate the communication between c++ and python at build-time
 * and to maintain only one source of truth (c++);
 */

#include <iostream>

#define PRINT_ENTRY(stencil_name) \
	std::cout << std::string{ (i == 0 ? "" : ", ") } + '"' << stencil_name << "\": " << i++ << "\n";

#define PRINT_NAME(instr_name) \
	std::cout << std::string{ (i++ == 0 ? "" : ", ") } + '"' << instr_name << '"' << "\n";

int main() {
	int i = 0;
	std::cout << "{\n\"opcodes\": {\n";
#define HANDLE_MICRO_INSTR(opcode) PRINT_ENTRY(#opcode)

#include "../../safe/low_program/micro_instruction_definitions.def.hpp"
#undef HANDLE_MICRO_INSTR

	PRINT_ENTRY("special_jump_if");
	PRINT_ENTRY("special_jump_if_not");
	PRINT_ENTRY("special_jump");
	PRINT_ENTRY("special_call_non_jittable");

	std::cout << "},\n\"nonjittable\": [\n";
	i = 0;
#define HANDLE_NONJITTABLE_INSTR(instr) PRINT_NAME(#instr)
#include "../non_jittable.def.hpp"
#undef HANDLE_NONJITTABLE_INSTR

	std::cout << "]\n}\n";
}
