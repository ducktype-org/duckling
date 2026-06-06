#include <iostream>

#define PRINT(stencil_name) \
	std::cout << std::string{ (i == 0 ? "" : ", ") } + '"' << stencil_name << "\": " << i++ << "\n";

int main() {
	int i = 0;
	std::cout << "{\n";
#define HANDLE_MICRO_INSTR(opcode) PRINT(#opcode)

#include "../../safe/low_program/micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR

	PRINT("special_return");
	PRINT("special_trampoline");
	PRINT("special_jump_if");
	PRINT("special_jump_if_not");
	PRINT("special_jump");
	PRINT("special_call_addr");

	std::cout << "}\n";
}
