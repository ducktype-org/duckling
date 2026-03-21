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

      public:
        ControlFlowAnalyzer(const low::LowFuncData& function) : function(std::cref(function)) {};
        ControlFlowAnalyzer() = delete;

        std::vector<CfOccurrence> controlFlowOccurrences();
        std::vector<usize> basicBlockBeginnings();

        ControlFlowGraph controlFlowGraph() {
            return ControlFlowGraph(this->function.get(), basicBlockBeginnings());
        }
    };
} // namespace vm::jit::cf
