/**
 * @file print_opcodes.cpp
 * @brief Print a json list of micro instruction names and corresponding opcode values. This
 * includes special stencils.
 * @details The goal here is to facilitate the communication between c++ and python at build-time
 * and to maintain only one source of truth (c++);
 */

#include <iostream>

#define PRINT(stencil_name) \
	std::cout << std::string{ (i == 0 ? "" : ", ") } + '"' << stencil_name << "\": " << i++ << "\n";

int main() {
	int i = 0;
	std::cout << "{\n";
#define HANDLE_MICRO_INSTR(opcode) PRINT(#opcode)

#include "../../safe/low_program/micro_instruction_definitions.def.hpp"
#undef HANDLE_MICRO_INSTR

	PRINT("special_jump_if");
	PRINT("special_jump_if_not");
	PRINT("special_jump");
	PRINT("special_call_non_jittable");

	std::cout << "}\n";
}
