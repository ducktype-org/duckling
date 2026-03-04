#pragma once

#include <vm/bytecode/bytecode.hpp>
#include <vector>

#ifdef ENABLE_JIT

namespace vm::jit {

enum class CfOccurrenceKind {
    Label,
    Jump,
    ConditionalJump,
};

struct CfOccurrence {
    usize            position;
    CfOccurrenceKind kind;

    CfOccurrence(usize position, CfOccurrenceKind kind): position(position), kind(kind) {}
};

struct DetectedLoop {
    usize start_position;
    usize end_position;
};
/**
 * @brief Collects all label and jump instruction occurrences in a function body.
 * Returns a sorted vector of control flow occurrences in the order they appear.
 *
 * @param function The function to analyze
 * @return Vector of control flow occurrences (labels and jumps)
 */
std::vector<CfOccurrence> collectControlFlowOccurrences(const code::Function& function);

} // namespace vm::jit

#endif // ENABLE_JIT
