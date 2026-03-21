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
      public:
        ControlFlowAnalyzer() = default;

        std::vector<CfOccurrence> controlFlowOccurrences(const low::LowFuncData& function);
        std::vector<usize> basicBlockBeginnings(const low::LowFuncData& function);

        ControlFlowGraph controlFlowGraph(const low::LowFuncData& function) {
            return ControlFlowGraph(function, basicBlockBeginnings(function));
        }
    };
} // namespace vm::jit::cf
