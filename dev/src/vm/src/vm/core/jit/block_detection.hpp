#pragma once

#ifdef ENABLE_JIT

#include <vector>
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

/**
 * @brief Collects all label and jump instruction occurrences in a function body.
 * Returns a sorted vector of control flow occurrences in the order they appear.
 *
 * @param function The function to analyze
 * @return Vector of control flow occurrences (labels and jumps)
 */
std::vector<CfOccurrence> collectControlFlowOccurrences(const low::LowFuncData& function);

/**
 * @brief Returns a sorted vector of basic block beginning positions with last postion equal to function body size
 *
 * @param function The function to analyze
 * @return Sorted vector of basic block beginning positions with last postion equal to function body size
 */
std::vector<usize> collectBasicBlockBeginnings(const low::LowFuncData& function);

} // namespace vm::jit

#endif // ENABLE_JIT
