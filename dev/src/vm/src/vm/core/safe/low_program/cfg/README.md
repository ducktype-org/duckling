# Control-Flow Graph and Loop Detection

The loop-compilation part of the JIT is described in detail in [the bachelor's thesis](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/lic/2026_JIT.pdf). The code in this directory builds and analyzes control-flow graphs for lowered bytecode. That CFG is used by function compilation, loop compilation, and can be reused by future control-flow driven passes.

## Control flow graph

The CFG is constructed from lowered function bytecode. Bytecode is split into basic blocks, with each jump, conditional jump, or return instruction becoming the end of a basic block, while each jump destination becoming the start of a block. Edges between basic blocks are based on possible jump paths and fallthrough behavior (for blocks ending on a non-jump, non-ret instruction). For a full function CFG, only blocks which end at the `ret` instruction will have no successor blocks.

This CFG structure serves as a simplified form of the loop body, designed to allow for easy conversion into different CFG representations, such as that used by the LLVM ORC compiler.

## Loop detection

Loop detection analysis follows a dominator-based approach. A back edge is treated as an edge `x -> y` where `y` dominates `x`, with `y` becoming the loop header. For each such header, all predecessors are traversed backwards to collect the loop body. In this model, the important property is single entry through the header, where we insert the loop's entrypoint and track execution counts.

The implementation uses an iterative immediate-dominator computation (with a dominator tree for fast dominance checks), which keeps the code simple and predictable for the CFG sizes we see in lowered functions.

## Loop artifacts and returned CFGs

At the API level, loop detection returns a vector indexed by instruction offset. For the function entrypoint offset, the slot contains the full function CFG. For loop headers, it contains extracted loop CFGs. Other offsets stay empty.

This layout keeps the full-function CFG available while attaching loop-specific artifacts only where loop headers exist. Runtime JIT entrypoint instructions can index JIT metadata directly by the current instruction position, without additional lookup structures.

When a loop CFG is extracted, outgoing edges leaving the loop are redirected to synthetic exit blocks. Those blocks encode the continuation target relative to the loop header. Compiled loop code returns that offset, and the interpreter uses it to restore correct control-flow on exit. This part is the key difference between the CFG of a full function, for which all exit blocks contain the `ret` opcode instruction, and a loop CFG, which needs to preserve information about where in the original function's CFG to exit to.

## Files

- `cf_analysis.hpp/.cpp` - basic-block beginnings and jump-target helpers.
- `cf_graph.hpp/.cpp` - CFG structures and loop subgraph extraction.
- `loop_detector.hpp` - dominator calculation and natural-loop detection.
