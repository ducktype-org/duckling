#pragma once

#include <functional>
#include <vector>

#include <vm/bytecode/bytecode.hpp>
#include <vm/core/jit/cf_graph.hpp>
#include <vm/core/thread/low_program/low_program.hpp>

namespace vm::jit::cf {
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

    class ControlFlowAnalyzer {
      private:
        std::reference_wrapper<const low::LowFuncData> function;

        // cached results
        std::vector<CfOccurrence> cf_occurrences;
        std::vector<usize>        block_beginnings;
        ControlFlowGraph          cfg;

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

        const ControlFlowGraph& getCFG() {
            if (this->cfg.size() == 0) {
                this->cfg = ControlFlowGraph(this->function.get(), getBlockBeginnings());
            }
            return cfg;
        }

      public:
        ControlFlowAnalyzer(const low::LowFuncData& function) : function(std::cref(function)) {};
        ControlFlowAnalyzer() = delete;

        std::vector<CfOccurrence> controlFlowOccurrences() {
            return getCfOccurrences();
        }
        std::vector<usize> basicBlockBeginnings() {
            return getBlockBeginnings();
        }
        ControlFlowGraph controlFlowGraph() {
            return getCFG();
        }
    };
} // namespace vm::jit::cf
