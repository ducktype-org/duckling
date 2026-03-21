#include "cf_analyzer.hpp"

#include <vm/bytecode/instructions.hpp>

#include <vm/core/thread/low_program/instruction.hpp>
#include <vm/core/thread/low_program/opcodes.hpp>

using namespace vm::code::instructions;

namespace vm::jit {
    std::vector<CfOccurrence> CfAnalyzer::collectControlFlowOccurrences() {
        if (!this->cf_occurrences.empty()) {
            // If occurrences have already been collected, return them
            return this->cf_occurrences;
        }

        auto function = this->function.get();
        for (usize index = 0; index < function.bc.size(); ++index) {
            low::MicroOpcode opcode = getInstructionOpcode(function.bc[index]);
            i64 arg0 = function.bc[index].arg0;

            switch(opcode) {
                case low::MicroOpcode::jmp_label: {
                    this->cf_occurrences.push_back(CfOccurrence{index, CfOccurrenceKind::Jump});
                    if (index + arg0 < function.bc.size()) {
                        this->cf_occurrences.push_back(CfOccurrence{index + arg0 + 1, CfOccurrenceKind::JumpDestination});
                    }
                    break;
                }
                case low::MicroOpcode::jmpIf_label:
                case low::MicroOpcode::jmpIfNot_label: {
                    this->cf_occurrences.push_back(CfOccurrence{index, CfOccurrenceKind::ConditionalJump});
                    if (index + arg0 < function.bc.size()) {
                        this->cf_occurrences.push_back(CfOccurrence{index + arg0 + 1, CfOccurrenceKind::JumpDestination});
                    }
                    break;
                }
                case low::MicroOpcode::ret:
                case low::MicroOpcode::ret_tailcall_func: {
                    this->cf_occurrences.push_back(CfOccurrence{index, CfOccurrenceKind::Ret});
                    break;
                }
                default: {
                    break;
                }
            }
        }

        // Remove duplicates (occur if there are many jumps to the same destination)
        std::sort(this->cf_occurrences.begin(), this->cf_occurrences.end());
        this->cf_occurrences.erase(
            std::unique(this->cf_occurrences.begin(), this->cf_occurrences.end()),
            this->cf_occurrences.end()
        );

        return cf_occurrences;
    }

    std::vector<usize> CfAnalyzer::collectBasicBlockBeginnings() {
        if (!this->block_beginnings.empty()) {
            return this->block_beginnings;
        }

        if (this->cf_occurrences.empty()) {
            this->collectControlFlowOccurrences();
        }
        this->block_beginnings = {0}; // First block always starts at position 0

        for (const auto& occurrence : this->cf_occurrences) {
            if (occurrence.kind == CfOccurrenceKind::JumpDestination) {
                // Jump destinations signify the beginning of a block
                if (this->block_beginnings.back() != occurrence.position) {
                    // Avoid adding duplicate block beginning if previous block ends in jump
                    // or it is at the start of the function
                    this->block_beginnings.push_back(occurrence.position);
                }
            } else {
                // Other instruction types signify the end of a block
                this->block_beginnings.push_back(occurrence.position + 1);
            }
        }

        return this->block_beginnings;
    }
} // namespace vm::jit
