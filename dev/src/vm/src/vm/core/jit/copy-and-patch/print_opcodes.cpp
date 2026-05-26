#include <vm/core/safe/low_program/opcodes.hpp>

#include <iostream>

int main () {
    std::cout << "{\n";
#define PRINT_OPCODE(opcode) std::cout << '"' << #opcode << '": ' << vm::low::MicroOpcode::##opcode << ",\n";
    #include <vm/code/safe/low_program/micro_instruction_definitions.hpp>
#undef PRINT_OPCODE
    std::cout << "}\n";
}