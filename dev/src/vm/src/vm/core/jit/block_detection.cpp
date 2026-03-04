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
            instr_default {}
        }
    }

    return out;
}

} // namespace vm::jit

#endif // ENABLE_JIT
