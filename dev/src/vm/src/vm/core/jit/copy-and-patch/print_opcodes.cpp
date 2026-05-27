#include <iostream>

int main() {
	int i = 0;
	std::cout << "{\n";
#define HANDLE_MICRO_INSTR(opcode) \
	std::cout << std::string{ (i == 0 ? "" : ", ") } + '"' << #opcode << "\": " << i++ << "\n";
#include "../../safe/low_program/micro_instruction_definitions.hpp"
#undef HANDLE_MICRO_INSTR
	std::cout << "}\n";
}
