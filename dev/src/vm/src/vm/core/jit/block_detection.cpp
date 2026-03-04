#include "block_detection.hpp"

#ifdef ENABLE_JIT

#include <vm/bytecode/instructions.hpp>
#include <base/extend_cpp/variant_match.hpp>

using namespace vm::code::instructions;

namespace vm::jit {

std::vector<CfOccurrence> collectControlFlowOccurrences(const code::Function& function) {
    std::vector<CfOccurrence> out;
    out.reserve(function.body.size());

    for (usize index = 0; index < function.body.size(); ++index) {
        instr_match(function.body[index]) {
            instr_case(Op_label, instr) {
                out.push_back(CfOccurrence{index, CfOccurrenceKind::Label});
            }
            instr_case(Op_jmp_label, instr) {
                out.push_back(CfOccurrence{index, CfOccurrenceKind::Jump});
            }
            instr_case(Op_jmpIf_label, instr) {
                out.push_back(CfOccurrence{index, CfOccurrenceKind::ConditionalJump});
            }
            instr_case(Op_jmpIfNot_label, instr) {
                out.push_back(CfOccurrence{index, CfOccurrenceKind::ConditionalJump});
            }
            instr_case(Op_ret, instr) {
                out.push_back(CfOccurrence{index, CfOccurrenceKind::Ret});
            }
            instr_case(Op_ret_tailcall_func, instr) {
                out.push_back(CfOccurrence{index, CfOccurrenceKind::Ret});
            }
            instr_default {}
        }
    }

    return out;
}

std::vector<usize> collectBasicBlockBeginnings(const code::Function& function) {
    std::vector<CfOccurrence> cf_occurrences = collectControlFlowOccurrences(function);
    std::vector<usize> block_beginnings(1, 0); // First block always starts at position 0

    for (const auto& occurrence : cf_occurrences) {
        if (occurrence.kind == CfOccurrenceKind::Label) {
            // Labels signify the beginning of a block
            if (block_beginnings.back() != occurrence.position) {
                // Avoid adding duplicate block beginning if previous block ends in jump
                // or label is at the start of the function
                block_beginnings.push_back(occurrence.position);
            }
        } else {
            // Other instruction types signify the end of a block
            block_beginnings.push_back(occurrence.position + 1);
        }
    }

    return block_beginnings;
}

} // namespace vm::jit

#endif // ENABLE_JIT
