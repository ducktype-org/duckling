# Fast mode

Fast mode (`VM run --fast-mode`, `api::ProcessMode::Fast`) is the second execution backend of the
DVM. It runs the same validated bytecode as [safe mode](../safe), but drops the runtime checks that
the bytecode validator or the high-level language compiler is responsible for. Its single goal is
speed, both interpreted and, above all, JIT-compiled.

This file is the design log of the backend: keep the assumptions, decisions and measurements below
up to date with every change that affects them.

## Goals

- Run every program safe mode runs, with the same observable behaviour for programs that safe mode
  completes without a runtime error.
- Be as fast as possible, and lower to code that LLVM and copy-and-patch can optimise well.
- Keep exceptions off the execution path.

## Non-goals

- Memory safety. Out-of-bounds accesses, use-after-free, double free, leaks and data races are
  undefined behaviour.
- Debugging and introspection: pause, step, breakpoints, stack frames, current position, mapping
  file lines to code positions.

## Architecture

```
ValidProgram ──FastCompiler──► reloc::Instruction ──relocator──► exec::Instruction ──FastExecutor──►
              (loader/compiler/fast)   (IDs)                       (raw pointers)    TC / SC dispatch
```

- `program/instructions/instruction_definitions.def.hpp` is the single list of fast instructions.
  Instruction names are mangled from their argument kinds (`add` + `Place64, Place64` is
  `add_p64_p64`); every instruction has at most 16 bytes of arguments.
- `reloc::Instruction` keeps compilation-stable IDs, `exec::Instruction` the resolved pointers, so
  that programs can be relinked after incremental loads.
- `eval/` holds the handlers (`FastExecutor::Instr_*`) and both dispatch strategies: tail calls
  (`dispatcher_*` in `executable.cpp`) and a switch loop (`evaluator.hpp`).

## Decision log

Newest last. Each entry: what was decided, why, and what would make us revisit it.

**D-1 (2026-11-13) Runtime errors are statuses, never exceptions.** Exception unwinding blocks LLVM
optimisations around every call that may throw, and copy-and-patch code has no unwind information,
so an exception crossing a stencil frame terminates the process. A failing handler sets a trap
kind in the thread runtime state and redirects execution to a terminating instruction; the thread
turns the trap into a `Panic`. Only instructions that can trap pay for it, and the JIT emits a
status check only after those.

**D-2 (2026-11-13) Only the checks the language cannot express are kept**: division and modulo by
zero, stack overflow (VM local stack and frame depth), calling a method on an object without a
vtable, and calling a null function pointer (`call_pfun`). Everything else, such as bounds, null
pointers, use-after-free and leaks, is the high-level compiler's job. Division by zero is kept for
now to measure what it costs the JIT; it is a candidate for removal.

**D-3 (2026-11-13) Native threads without a GIL.** Fast mode emits no `stepGil`. Programs that race
on data have undefined behaviour.

**D-4 (2026-11-13) Interrupt checks are a process option, off by default.** With the option enabled,
lowering emits `checkStrategy` before back-edges and calls, so `stop()` interrupts any running
code through a trap. With it disabled, `stop()` is best effort: only blocking waits (IO, mutexes,
condition variables) observe it, and a busy loop runs to completion.

**D-5 (2026-11-13) Debugger and introspection APIs return `NotImplementedError`** without throwing.
The CLI already rejects `--debug` together with `--fast-mode`.

**D-6 (2026-11-13) Exception freedom is enforced on the IR, not with `-fno-exceptions`.** Shared
headers (`base/str`, `base/collections`, `string_id`, `safe/memory/pointer.hpp` through the API
types) contain `throw` expressions, so the handlers cannot be compiled with `-fno-exceptions`.
Handlers are `noexcept`, and `./toolbox.py vm ir-stats --check-no-unwind` fails if any handler
contains `invoke`, `landingpad`, `resume` or a throw. Every function a handler calls must not
throw: use `malloc` rather than `new`, `std::from_chars` rather than `std::stoll`, and assume
external C and FFI functions are `noexcept`.

**D-7 (2026-11-13) Pointers are raw 8-byte addresses.** The validator already models an 8-byte
pointer size (`TypeSize::assumePointerSize`). `alloc` uses `calloc` and `free` uses `free`;
C pointers and VM pointers behave the same. A dynamic table is laid out as `[u64 count | data]`
and the pointer points at the data; the count is used only by operations that need the length,
such as `strOutput`, reallocation and `fstToDynTable`.

**D-8 (2026-11-13) Places keep the bit-63 global flag** (`utils.hpp`, `encodePlace`), decoded with a
branchless select. In JIT code the argument is a constant, so the select folds away. Revisit if
`vm ir-stats` shows a measurable interpreter cost.

**D-9 (2026-11-13) Virtual calls use dense vtables.** An object's vtable is an array of
`const ExecFunction*` indexed by a global method id resolved at lowering time, so a virtual call is
two loads and an indirect call, with no hashing. `downcast` uses a precomputed set of ancestor
type ids.

**D-10 (2026-11-13) JIT: LLVM tier plus copy-and-patch, built from the same handlers.** Both tiers
inline the handler bodies with instruction arguments as constants. A compiled function has the ABI
`u64 fn(ThreadRuntimeState*, byte* local_stack, FastVMThread*)` and returns a status. Interpreted
and compiled code call each other directly through an `entry` pointer on every function, without
safe mode's trampoline that interprets a copied call instruction. Because no exceptions cross JIT
frames (D-1, D-6), copy-and-patch code needs no unwind information. Debuggers and `perf` still
cannot unwind through it, which is a tooling limitation only.

## Known limitations

- No interrupt checks by default (D-4): `stop()` does not interrupt busy loops.
- 22 of 387 bytecode instructions are lowered. Any other instruction aborts lowering.

## Tooling

- `./toolbox.py vm ir-stats [-v tc|sc] [--calls] [--mca] [--json out.json] [--compare base.json]
  [--check-no-unwind]` compiles the handlers with the repository's clang at `-O3` and reports, per
  handler and dispatcher, the LLVM IR instruction count, basic blocks, x86 instructions, bytes,
  calls that were not inlined and unwinding code. By default the translation units are linked and
  optimised together, which emulates LTO; `--no-link` shows what a build without LTO gets.
- `../benchmark` (the `ducktype-org` benchmark repository, a sibling of the `duckling` checkout)
  measures retired user-space instructions with `perf stat`. The `vm_fast_*` configs run fast mode;
  `quick_perf.sh` accepts `MODE=--fast-mode`.

## Findings

**F-1 (2026-11-13) Without LTO every TC dispatcher calls its handler out of line.** Handlers
(`instruction_functions.cpp`) and dispatchers (`executable.cpp`) live in different translation
units, and `build-release` (`ReleaseOpt`) has `ENABLE_LINK_TIME_OPTIMIZATION=OFF`, so every
instruction costs a call plus the tail call (`vm ir-stats -v tc --no-link`). As a result fast TC
is slower than fast SC, and on fib_iter even slower than safe TC (see the performance log). The
handler bodies must be visible to the dispatchers.

**F-2 (2026-11-13) Advancing the instruction pointer reloads the top frame.** `PROGRESS_BY` goes
through `state.top_frame->ip` instead of the `frame` pointer that is already in a register, which
adds two dependent loads per instruction.

## Performance log

Retired user-space instructions (`perf stat -e instructions:u`, 3 runs, pinned to one core),
`build-release` (`ReleaseOpt`, GCC 16, no LTO), interpreted. Programs are in `src/vm/tests`:
`runtime/correctness/ackermann_new.dbc`, `common/programs/fib_iter.dbc` and
`common/programs/fib_rec.dbc`. `bin/VM` is the switch-case (SC) variant.

| Date | Mode | Variant | ackermann_new `3 10` | fib_iter `50000000 1000000` | fib_rec `32` | fib_rec `35` |
|---|---|---|---|---|---|---|
| 2026-11-13 | safe | SC | 29.89 G | 19.83 G | 4.09 G | 17.24 G |
| 2026-11-13 | safe | TC | 21.75 G | 14.48 G | 3.02 G | 12.69 G |
| 2026-11-13 | fast | SC | 18.18 G | 17.28 G | 2.52 G | 10.57 G |
| 2026-11-13 | fast | TC | 19.79 G | 18.63 G | 2.74 G | 11.51 G |
