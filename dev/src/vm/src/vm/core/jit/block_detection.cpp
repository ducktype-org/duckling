#include "block_detection.hpp"

#ifdef ENABLE_JIT

#include <iostream>

#include <vm/bytecode/instructions.hpp>

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>

using namespace vm::code::instructions;

namespace vm::jit {

std::vector<CfOccurrence> collectControlFlowOccurrences(const low::LowFuncData& function) {
    std::vector<CfOccurrence> out;
    std::cerr << "Collecting control flow occurrences for function with " << function.bc.size() << " bytecode instructions.\n";

    for (usize index = 0; index < function.bc.size(); ++index) {
        low::MicroOpcode opcode = getInstructionOpcode(function.bc[index]);
        u64 arg0 = function.bc[index].arg0;
        u64 arg1 = function.bc[index].arg1;
        std::string repr = function.bc[index].representation;

        std::cerr << "Instruction at " << index <<" - "<< repr << ": opcode=" << int(opcode) << ", arg0=" << arg0 << ", arg1=" << arg1 << "\n";

        switch(opcode) {
            case low::MicroOpcode::jmp_label: {
                std::cerr << "Found jump at index " << index << " with target offset " << arg0 << "\n";
                out.push_back(CfOccurrence{index, CfOccurrenceKind::Jump});
                if (index + arg0 < function.bc.size()) {
                    out.push_back(CfOccurrence{index + arg0 + 1, CfOccurrenceKind::JumpDestination});
                }
                break;
            }
            case low::MicroOpcode::jmpIf_label:
            case low::MicroOpcode::jmpIfNot_label: {
                std::cerr << "Found conditional jump at index " << index << " with target offset " << arg0 << "\n";
                out.push_back(CfOccurrence{index, CfOccurrenceKind::ConditionalJump});
                if (index + arg0 < function.bc.size()) {
                    out.push_back(CfOccurrence{index + arg0 + 1, CfOccurrenceKind::JumpDestination});
                }
                break;
            }
            case low::MicroOpcode::ret:
            case low::MicroOpcode::ret_tailcall_func: {
                std::cerr << "Found return at index " << index << "\n";
                out.push_back(CfOccurrence{index, CfOccurrenceKind::Ret});
                break;
            }
            default: {
                break;
            }
        }
    }

    // Remove duplicates (occur if there are many jumps to the same destination)
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());

    std::cerr << "Collected control flow occurrences:\n";
    for (const auto& occurrence : out) {
        std::cerr << "  Position: " << occurrence.position << ", Kind: " << static_cast<int>(occurrence.kind) << "\n";
    }

    return out;
}

std::vector<usize> collectBasicBlockBeginnings(const low::LowFuncData& function) {
    std::vector<CfOccurrence> cf_occurrences = collectControlFlowOccurrences(function);
    std::vector<usize> block_beginnings(1, 0); // First block always starts at position 0

    for (const auto& occurrence : cf_occurrences) {
        if (occurrence.kind == CfOccurrenceKind::JumpDestination) {
            // Jump destinations signify the beginning of a block
            if (block_beginnings.back() != occurrence.position) {
                // Avoid adding duplicate block beginning if previous block ends in jump
                // or it is at the start of the function
                block_beginnings.push_back(occurrence.position);
            }
        } else {
            // Other instruction types signify the end of a block
            block_beginnings.push_back(occurrence.position + 1);
        }
    }

    std::cerr << "Collected basic block beginnings at positions:\n";
    for (usize pos : block_beginnings) {
        std::cerr << "  " << pos << "\n";
    }

    return block_beginnings;
}

} // namespace vm::jit

#endif // ENABLE_JIT
