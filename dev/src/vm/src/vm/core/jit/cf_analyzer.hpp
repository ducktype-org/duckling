#pragma once

#include <vector>
#include <functional>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/thread/low_program/low_program.hpp>

namespace vm::jit {
    enum class CfOccurrenceKind {
        JumpDestination,
        Jump,
        ConditionalJump,
        Ret,
    };

    struct CfOccurrence {
        usize            position;
        CfOccurrenceKind kind;

        CfOccurrence(usize position, CfOccurrenceKind kind): position(position), kind(kind) {}
        auto operator<=>(const CfOccurrence&) const = default;
    };

    class CfAnalyzer {
      private:
        std::reference_wrapper<const low::LowFuncData> function;
        std::vector<CfOccurrence>                      cf_occurrences;
        std::vector<usize>                             block_beginnings;

        void calcControlFlowOccurrences();
        void calcBasicBlockBeginnings();

        const std::vector<CfOccurrence>& getCfOccurrences() {
            if (this->cf_occurrences.empty()) {
                this->calcControlFlowOccurrences();
            }
            return cf_occurrences;
        }

        const std::vector<usize>& getBlockBeginnings() {
            if (this->block_beginnings.empty()) {
                this->calcBasicBlockBeginnings();
            }
            return block_beginnings;
        }

      public:
        CfAnalyzer(const low::LowFuncData& function) : function(std::cref(function)) {};
        CfAnalyzer() = delete;

        std::vector<CfOccurrence> collectControlFlowOccurrences() {
            return getCfOccurrences();
        }
        std::vector<usize> collectBasicBlockBeginnings() {
            return getBlockBeginnings();
        }
    };
} // namespace vm::jit
