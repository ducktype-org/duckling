#pragma once

#define USE_SWITCH_CASE 1
#include <ostream>
#include <vm/core/safe/low_program/low_program.hpp>
#include <vm/core/safe/low_program/instruction.hpp>
#include <vm/core/safe/low_program/opcodes.hpp>
#include <vm/core/safe/opcode_functions/opcodes_functions.hpp>


namespace vm::code {

/// Print micro bytecode line-by-line with instruction names and arguments
inline void printMicroBytecode(
    const low::LowFuncData& func_data,
    std::ostream& out,
    const std::string& indent = ""
) {
    out << indent << "Function: " << func_data.name.strView() << " (id=" << func_data.id << ")\n";
    out << indent << "Parameters: " << func_data.arg_size << ", Returns: " << func_data.ret_size << "\n";
    out << indent << "Local Stack Size: " << func_data.local_stack_size << ", Block Count: " << func_data.local_block_count << "\n";
    out << indent << "Bytecode:\n";

    const auto& micro_bc = func_data.bc;

    for (usize i = 0; i < micro_bc.size(); ++i) {
        const auto& instruction = micro_bc[i];

        // Extract opcode
        auto opcode = getInstructionOpcode(instruction);
        usize opcode_index = static_cast<usize>(opcode);

        // Get opcode name
        std::string_view opcode_name = 
            (opcode_index < low::OPCODE_NAMES.size()) 
                ? low::OPCODE_NAMES[opcode_index]
                : "UNKNOWN";

        // Access arguments
        u64 arg0 = instruction.arg0;
        u64 arg1 = instruction.arg1;

        out << indent << "  [" << i << "] " << opcode_name
            << " arg0=" << arg0 << " arg1=" << arg1;

#ifdef BUILD_TYPE_DEV_DEBUG
        if (!instruction.representation.empty()) {
            out << " (" << instruction.representation << ")";
        }
#endif

        out << "\n";
    }
}

/// Print micro bytecode with control flow graph information (if JIT enabled)
// inline void printMicroBytecodeWithCFG(
//     const low::LowFuncData& func_data,
//     std::ostream& out,
//     const std::string& indent = ""
// ) {
//     printMicroBytecode(func_data, out, indent);

// #ifdef ENABLE_JIT
//     if (!func_data.cfgs.empty()) {
//         out << indent << "Control Flow Graphs:\n";
//         for (usize i = 0; i < func_data.cfgs.size(); ++i) {
//             out << indent << "  CFG[" << i << "]: " << func_data.cfgs[i].toString() << "\n";
//         }
//     }
// #endif
// }

/// Print instruction mapping from FatBytecode to MicroBytecode
inline void printInstructionMapping(
    const low::LowFuncData& func_data,
    std::ostream& out,
    const std::string& indent = ""
) {
    out << indent << "Instruction Mapping (FatBytecode -> MicroBytecode):\n";
    const auto& mapping = func_data.instruction_mapping;

    for (usize i = 0; i < mapping.size(); ++i) {
        const auto& range = mapping[i];
        out << indent << "  FatBC[" << i << "] -> MicroBC[" << range.begin << ", " << range.end << ")\n";
    }
}

} // namespace vm::code
