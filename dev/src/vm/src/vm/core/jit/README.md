# JIT compiler

The two tiered, method-based JIT compiler is described in detail in [the bachelor's thesis](https://github.com/ducktype-org/dev-space/blob/main/prace_naukowe/lic/2026_JIT.pdf). The first, [lightweight tier](./copy-and-patch/README.md) uses the innovative Copy-and-Patch technique, while the other, [LLVM-based compiler](./llvm/README.md) is designed to optimize the code as much as possible under the project's assumptions.

## Compilation outline

Both compilers view the user code as a list of calls to micro instruction implementations, and inline those calls. Additionally, they handle the previously generated [CFG](https://en.wikipedia.org/wiki/Control-flow_graph), to ensure proper execution order, as the instructions themselves (eg. `jumpIf`) do not actually transfer the execution to the appropriate place, but only set the instruction pointer.

Additionally to facilitate the control-flow back to interpreter, we use a [trampoline](./jit_helper.cpp). It starts a new interpreter loop, on a start function consisting of a call instruction, and an exit. This, of course is not expected when calling an already compiled function, and should be expanded on later. Especially strongly connected components of recursive functions may be compiled, and optimized together.

## Compilation target

The loops [discovered right after bytecode compilation](../safe/low_program/cfg/README.md), as well as whole functions are targets to compilation. Right now this is impaired in two ways: 

1. lack of inter-function optimization (to reuse compiled loops in the compilation of a greater loop)
2. the inability of the Copy-and-Patch compiled function to later invoke the LLVM compilation (of loops specifically) and run that binary.

## Entrypoints

The entrypoints are instructions inserted under the loop or function head. Due to the aforementioned issues, the two entrypoints (function and loop) differ based on whether both, or only LLVM are enabled. The entrypoints count down from the given threshold, until the first compiler is invoked. In both compilers mode only count down from the second threshold, while executing the binary artifact of the previous compilation. 

Since the compiled functions run a trampoline to execute a different function, they can trigger the compilation as well as execute the previously optimized code of functions (including itself).

## Bitcode generation

To generate quality implementation bitcode for the LLVM (and later the Copy-and-Patch compiler) we run an additional compilation process and perform manual link-time optimization, by joining the whole VM in a single `.bc` file. Before optimizing, only instruction implementations are marked as external, allowing for aggressive dead-code elimination.

### Non-jittable instructions

Some instruction implementations are quite large (and take up more compilation time) or complex (and inhibit optimizations) — the prime example being function calls, which transfer control back to the interpreter anyway. These are marked as non-jittable: the JIT tiers never compile their bodies and instead emit a plain call to the debug-mode implementation (via the `special_call_non_jittable` stencil in Copy-and-Patch, or an absolute symbol in the LLVM tier). This reduces startup and compilation times, and can actually speed up the generated code.

The list lives in [non_jittable.def.hpp](./non_jittable.def.hpp) and is the single source of truth: C++ includes it directly, while the python build scripts receive it as JSON (inside `opcodes.json`, produced by `print_opcodes`). Currently this marking is quite restricted, and the JIT compiler would perhaps benefit from expanding the list of non-jittable instructions to most non-arithmetic instructions.
