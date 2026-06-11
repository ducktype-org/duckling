# Psacklinger — a PSAC-parallelized Duckling benchmark compiler

Psacklinger is a small, self-contained compiler for a subset of Duckling, built to
benchmark **PSAC** (Parallel Self-Adjusting Computation,
[paper](https://dl.acm.org/doi/10.1145/3409964.3461799),
[implementation](https://github.com/cmuparlay/psac)) as the engine of a compiler
pipeline. It compiles each function **in parallel** and, when fed new source,
recompiles **only the functions that changed**.

Supported language subset: functions (calls, recursion, forward references, void),
`i64`/`bool`, classes with fields, `if`/`while`, compile-time `const`s, global
variables, namespaces (accessed with `.`), arithmetic/comparison/logical operators
and compound assignments.

## Pipeline

![pipeline](docs/pipeline.svg)

1. **Parse** the source into a PST (sequential).
2. **GlobalPass** registers all top-level declarations — namespaces, function
   signatures, class layouts, consts, globals — into `Context`, a pure-data
   symbol/type/namespace table (`psackling/context.hpp`). This is cheap and stays
   sequential.
3. **FunctionCollector** walks the PST and turns every function definition into a
   *work item*: the `pst::Fun` node, its enclosing namespace, a content hash, and
   its namespace-qualified link name.
4. **PSAC computation** (`psackling/compiler.cpp`, `compileFunctionsPsac`) compiles
   every work item as an independent parallel task. Each task emits its own object
   file `<output>.fnN.o`.
5. A dedicated `<output>.globals.o` defines all global variables (with
   compile-time-evaluated initializers); function modules only *declare* the
   globals and functions they reference.
6. **Link**: one `clang` invocation over all object files produces the executable.

## How PSAC is used

The whole back-end is one self-adjusting computation:

```cpp
psac_function(compileFunctionsPsac, /* pointers to shared state */) {
    psac_parallel_for(u64 i, 0, n, /*granularity=*/1, {
        psac_read((u64 fn_hash, u64 globals_hash), (&fn_mods[i], globals_mod), {
            compileOneFunction(*base_ctx, items[i], object_paths[i]);
        });
    });
}
```

- Every function gets a **`psac::Mod<u64>` holding its content hash**; one extra
  Mod holds the **globals hash** (classes, consts, global variables, and the
  *signatures* of all functions).
- Each function's compilation is a **`psac_read` of exactly those two Mods**. The
  read body is the dependency declaration: "this object file is a function of
  these two values".
- `psac_run` executes the computation once — that is the parallel full build. The
  returned `psac::Computation` (the dynamic dependency graph) is **kept alive** in
  `pg::Compiler`.
- On an update, the driver rewrites only the Mods whose hashes changed and calls
  `psac_propagate`, which re-executes exactly the dirty reads — again in parallel
  if several functions changed.

Content hashes are derived from each PST element's recursive debug print, which
contains the full structure, names and values but **no source positions or file
identity** — so reformatting, moving a function, or recompiling from a different
file does not invalidate anything.

## How the parallelism works

![sp tree](docs/sp_tree.svg)

Two layers:

**PSAC layer — the SP tree.** PSAC programs are series-parallel: `psac_parallel_for`
recursively splits its range (granularity 1 → every function is a leaf) and forks
the halves with `parlay::par_do`, building a tree of P-nodes whose leaves are the
`psac_read` R-nodes. This tree is not just the execution plan — it is *retained*
as the dependency structure, which is what makes change propagation work: a
written Mod marks its reader R-nodes dirty, and propagation walks the SP tree
re-running only dirty subtrees (in parallel, with the same forking).

**Scheduler layer — fork-join work stealing.** `parlay`'s scheduler spawns N
worker threads (`-j/--threads`, default `PARLAY_NUM_THREADS` or hardware
concurrency). Each fork pushes one branch onto the calling worker's local deque
and runs the other branch immediately; idle workers steal from the top of random
victims' deques. Nothing is statically partitioned — load balancing is dynamic, so
one huge function does not stall the others.

**What made per-function compilation safe to run concurrently:**

- `Context` is decoupled from LLVM and cheaply copyable — each task takes a
  private copy, because lowering inserts local-variable symbols into it.
- Each task owns a private `llvm::LLVMContext` + `llvm::Module`
  (LLVM contexts are not thread-safe). Cross-function calls and global-variable
  accesses become external declarations with namespace-qualified link names
  (`math.add`), resolved by the system linker.
- One-time LLVM target initialization is guarded by `std::call_once`; symbol IDs
  use atomic counters; `StrID` interning is thread-safe.
- Exceptions never unwind into the scheduler: each task catches and records its
  error, and the driver rethrows after the run.

## Incremental recompilation

![incremental](docs/incremental.svg)

`pg::Compiler` keeps the computation alive between compiles:

- `compile(file)` — full build (`psac_run`).
- `update(file)` — parse + re-analyze the new source, then **diff the hashes**:
  - body changed → that function's Mod is rewritten → propagation recompiles
    exactly that function;
  - const/class/global/signature changed → the globals Mod is rewritten → every
    function recompiles (the coarse half of dependency tracking; the real compiler
    tracks fine-grained query inputs instead);
  - function added/removed/renamed → the computation shape no longer matches →
    automatic full rebuild;
  - nothing changed → nothing recompiled, no relink.

This mirrors the production compiler's incremental design in miniature: there,
queries depend on per-PST-element stable hashes collected as side inputs
(`driver/incremental_utils/`), and the query framework invalidates whatever
depended on a changed hash. Here, "query" = one function's compilation, and the
"side inputs" are the two Mods it reads.

## Usage

```sh
psacklinger prog.dmf -o prog.out            # parallel build
psacklinger prog.dmf -j 4                   # 4 worker threads
psacklinger prog.dmf --sequential           # no PSAC, baseline for benchmarks
psacklinger prog.dmf -i                     # interactive incremental mode
```

Interactive mode reads source paths from stdin after the initial compile — each
line triggers an incremental update (empty line repeats the previous path, `quit`
exits) and prints `recompiled K/N functions`:

```
$ psacklinger a.dmf -o a.out -i
recompiled 2/2 functions
b.dmf            # edited helper() body
recompiled 1/2 functions

recompiled 0/2 functions
```

## Tests

`psacklinger_test` (target `psacklinger_test`) runs ~80 end-to-end tests: each
compiles a program through the full parallel pipeline, executes it and checks the
exit code. The `Incremental*` tests assert the recompile *counts* — e.g. that a
one-body edit recompiles exactly one function and an unchanged source recompiles
zero.

## Known limitations

- No short-circuit `and`/`or`, named/default arguments, subscripts or methods.
- Global initializers must be compile-time evaluable scalars; struct globals are
  zero-initialized.
- Globals dependency is coarse (one hash for everything non-body).
- Templates (`template{...}`) are not implemented — the Duckling frontend has no
  `template` keyword yet; only the baking-site expression `name:{args}` parses.
