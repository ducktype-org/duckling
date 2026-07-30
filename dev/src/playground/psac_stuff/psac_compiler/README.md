# Psacklinger — a PSAC-parallelized Duckling benchmark compiler

Psacklinger is a small, self-contained compiler for a subset of Duckling, built to
benchmark **PSAC** (Parallel Self-Adjusting Computation,
[paper](https://dl.acm.org/doi/10.1145/3409964.3461799),
[implementation](https://github.com/cmuparlay/psac)) as the engine of a compiler
pipeline. It compiles each function **in parallel** and, when fed new source,
recompiles **only the functions that changed**.

Supported language subset: functions (calls, recursion, forward references, void),
`i64`/`bool`/`char` (char literals with C-style escapes), classes with fields and
default constructors (`Point()`), C-style arrays (`i64[10]`, multidimensional
`i64[3][4]`, C99-style VLAs `i64[n]` for locals; usable as locals, globals and
class fields — no pass/return by value or whole-array assignment, like C),
`if`/`while`, compile-time `const`s, global
variables with runtime initializers, namespaces (accessed with `.`),
arithmetic/comparison/logical operators, compound assignments and the
`builtin_output_i64`/`builtin_output_char` runtime builtins. Enough to compile the
~8000-line generated benchmark samples.

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
4. **PSAC computations** (`psackling/compiler.cpp`, `compileUnitPsac`) compile
   every work item as an independent parallel task — one self-adjusting
   computation per function. Each task emits its own object file
   `<output>.fn_<link-hash>.o` (named by link-name hash, so the file identity
   survives reordering).
5. A dedicated `<output>.globals.o` defines all global variables
   (zero-initialized) plus the runtime builtins; global initializer expressions —
   which may call functions and read other globals — are lowered in declaration
   order into a synthetic constructor registered in `llvm.global_ctors`, so libc
   runs it before `main`. Function modules only *declare* the globals and
   functions they reference.
6. **Link**: one `clang` invocation over all object files produces the executable.

## How PSAC is used

Every function is its own self-adjusting computation, built around a dynamic-read
scope:

```cpp
psac_function(compileUnitPsac, State* s, FunctionUnit* unit, std::string link_name) {
    psac_dynamic_context({
        _psac_context_mods.push_back(&unit->body_mod);
        _psac_context_mods.push_back(&s->structural_mod);
        DynamicDepRecorder recorder(_psac_context_mods, *s, s->ctx);
        compileOneFunction(s->ctx, item, unit->object_path, &recorder);  // + error handling
    });
}
```

- Every function gets a **`psac::Mod<Bit256>` holding its body-content hash**, and
  every global symbol (constant, function signature, class layout, global
  variable) gets its own Mod holding its value hash — the PSAC analog of
  querylinger's per-symbol side inputs. One structural Mod covers imports,
  namespace names and module identity.
- Each function's compilation is a **PSAC dynamic-read scope** over its body Mod,
  the structural Mod, and — registered at the use site through
  `pg::SymbolUseObserver` while lowering runs — the Mods of exactly the symbols
  it used. The scope's subscription set is re-derived on every re-run, so
  dependencies stay precise as the code evolves.
- A **failed** compilation instead subscribes to the whole-world Mod: its true
  dependency set was never discovered (e.g. the missing function it called), so
  it is retried whenever anything on the global surface changes — negative
  dependencies, coarse but sound.
- `psac_run` per function executes the cold build (all functions in a
  `parlay::parallel_for`); the returned `psac::Computation`s are **kept alive** in
  `pg::Compiler`. Because each function owns its computation, adding or removing
  functions just adds or destroys computations — no full rebuild.
- On an update, the driver rewrites only the Mods whose hashes changed
  (`Mod::write` self-diffs) and propagates every computation; clean ones are
  no-ops, dirty ones re-run in parallel. A symbol that vanished gets a tombstone
  value, so its readers re-run and report the missing symbol.

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

**Scheduler parking.** parlay's worker pool is created during *static
initialization* (a global `block_allocator` calls `parallel_for` before `main`)
and its idle workers busy-spin. Left alone, 16 spinning threads thrash caches and
cores through the entire serial front-end, roughly doubling parse time. The driver
therefore parks the pool at a single worker (`parlay::set_num_workers(1)`) and
expands it only around `psac_run`/`psac_propagate`. This one fix roughly halved
full-compile wall time (first measured on Debug builds: ~6 s → ~2.8 s).

**Building with clang.** The vendored psac headers did not compile under clang
(`Computation`'s in-class constructor/destructor instantiate
`unique_ptr<SPNode>` while `SPNode` is still incomplete; GCC defers the
instantiation, clang rejects it). `dependencies/psac_clang_computation_fix.patch`
moves those definitions after `SPNode` and is applied automatically by
`dependencies/Psac.cmake`.

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

- `compile(file)`/`update(file)` — both diff-driven: parse + re-analyze the new
  source, rewrite the Mods whose hashes changed, reconcile the function set, and
  propagate:
  - body changed → that function's Mod is rewritten → propagation recompiles
    exactly that function;
  - const/class/global-var/signature changed → that symbol's Mod is rewritten →
    exactly its users recompile (parameter renames touch nothing: signature
    hashes cover types, not names);
  - global variable *initializer* changed → only the globals object is re-emitted
    and relinked; readers depend on the variable's identity and type alone;
  - function added/removed/renamed → a computation is created or destroyed; the
    survivors stay cached;
  - a previously failed function is retried when the global surface changes (its
    failure subscribed to the whole-world Mod);
  - nothing changed → nothing recompiled, no relink.

This mirrors the production compiler's incremental design in miniature: there,
queries depend on per-PST-element stable hashes collected as side inputs
(`driver/incremental_utils/`), and the query framework invalidates whatever
depended on a changed hash. Here, "query" = one function's compilation, and the
"side inputs" are the Mods its dynamic-read scope subscribed to.

## Usage

```sh
psacklinger prog.dmf -o prog.out            # parallel build
psacklinger prog.dmf -j 4                   # 4 worker threads
psacklinger prog.dmf --sequential           # no PSAC, baseline for benchmarks
psacklinger prog.dmf -i                     # interactive incremental mode
PSACKLINGER_TIMING=1 psacklinger prog.dmf   # phase timings on stderr
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

## Benchmarks vs duckc and querylinger

`./bench_compiler.sh [input.dmf ...]` builds the three compilers (release builds
from `build-release` by default; `BUILD_DIR` overridable) and runs hyperfine
over:

- `duckc` — `compile_module`, single-threaded, object files only;
- `duckc-wN` — `compile_packages -w N` (query-framework workers); also compiles
  the std/core packages and links a real executable, the closest equivalent of
  what psacklinger does;
- `psacklinger --sequential` and `psacklinger -j N`;
- `querylinger -j N` — the query-framework sibling (`../query_compiler`),
  sharing this compiler's whole front end and LLVM lowering, with the query
  graph replacing PSAC as the incremental engine.

Everything is cold: duckc's incremental cache and artifacts are wiped before
every timed run. `THREADS`, `RUNS`, `WARMUP` are env-tunable; results land next
to the script as `results_compiler_<name>.{json,md}`.

Release builds, ~8000-line generated samples (`benchmark/data/input`), 5 runs,
per-symbol engines on both sides:

| threads | duck1: duckc -w | duck1: psack -j | duck1: query -j | duck2: duckc -w | duck2: psack -j | duck2: query -j |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 764 ms | 589 ms | 545 ms | 1164 ms | 459 ms | 437 ms |
| 2 | 706 ms | 417 ms | 369 ms | 1159 ms | 379 ms | 360 ms |
| 4 | 618 ms | 333 ms | 294 ms | 1116 ms | 345 ms | 315 ms |
| 8 | 585 ms | 295 ms | 254 ms | 1057 ms | 334 ms | 297 ms |
| 16 | 569 ms | 284 ms | **239 ms** | 1074 ms | 335 ms | **302 ms** |

Readings:

- Both playground compilers beat duckc at every width (~2.4× on duck1, ~3.6× on
  duck2 at -j8) — per-function fan-out vs duckc's serial task chain on a single
  big module.
- querylinger edges out psacklinger at every width (~5–20%): the query
  framework's task pool costs no more than PSAC's per-function computations at
  this granularity, and nothing from parlay links into the querylinger binary,
  so its busy-spin scheduler can't touch the serial front end at all.
- The per-symbol engine rework (one computation per function, dynamic-read
  scopes, per-symbol Mods) left psacklinger's cold numbers within a few percent
  of the old coarse engine.
- Build type matters enormously: the same benchmark on Debug builds reads
  several times slower — only relative trends survive, absolute Debug numbers
  don't.
- Why duckc barely scales with `-w` (`--print-statistics`, duck1): PST
  construction — parse plus per-element SHA-256 stable hashing — is ~232 ms and
  fully serial, about half of the `-w8` wall time; the parallel query phase
  sits mostly in the name-resolution chain (`QueryLookupInScopeAndParents`,
  `QueryScopesInModule`, `QueryPrimaryCodeScopeFor`). Parallelizing PST
  stable-hash computation is the biggest single lever (the psacklinger
  `PSTHashing::Skip` experiment showed 202 ms → 67 ms when hashing is removed
  entirely).

Warm incremental updates (`bench_incremental.py`, interactive mode, -j8,
single-line edits; see `results_incremental_<name>.md`), both engines on
per-symbol dependencies:

| scenario | psacklinger | querylinger |
|:---|:---|:---|
| no-op | ~105 ms, 0/352 | ~115 ms, 0/352 |
| one body edit | ~151 ms, 1/352 | ~163 ms, 1/352 |
| global var initializer | ~152 ms, **0/352** | ~160 ms, **0/352** |

(The old coarse psacklinger engine recompiled **352/352** on the global-var
row at ~220 ms; the per-symbol Mods eliminated that.) The ~100 ms floor is the
shared non-incremental front end (full re-parse + re-analyze). At equal
dependency granularity the engines are indistinguishable — psacklinger is now
marginally ahead warm, querylinger cold (see
`../query_compiler/docs/architecture.md`).

Phase breakdown (`PSACKLINGER_TIMING=1`, duck1 at `-j8`, release): parse 67 ms,
GlobalPass 1 ms, collect+hash 22 ms, globals object 2 ms, function lowering
58 ms, link 55 ms. Two front-end optimizations got it here:

- **Scheduler parking** (above) stopped idle workers from thrashing the serial
  phase.
- **`pst::PSTHashing::Skip`**: the PST constructor normally computes SHA-256
  stable hashes for every element (the query framework's incremental inputs).
  psacklinger keeps its own dprint-based content hashes, so it opts out via a
  new flag on the `pst::PST` constructor — parse dropped 202 ms → 67 ms.
  `getHash()`/`getByStableHash()` must not be used on such a PST; the default
  is unchanged for the rest of the compiler.

Where the PSAC design pays off unconditionally is incremental recompiles: a
one-function edit re-runs one ~ms-scale task plus a relink instead of a full
compile.

## Tests

`psacklinger_test` (target `psacklinger_test`) runs ~108 end-to-end tests: each
compiles a program through the full parallel pipeline, executes it and checks the
exit code. The program corpus lives in `../compiler_test_corpus/` and is shared
with `querylinger_test`, so both compilers prove identical language behavior.
The `Incremental*` tests assert the recompile *counts* — e.g. that a one-body
edit recompiles exactly one function and an unchanged source recompiles zero.

## Known limitations

- No short-circuit `and`/`or`, named/default arguments, subscripts or methods;
  constructors take no arguments (zero-initialization only).
- `char` compares (unsigned byte order) but has no arithmetic and no `char`↔`i64`
  casts, mirroring the real compiler's distinct char type.
- Import/namespace rewiring stays coarse (the structural Mod): it can change what
  any name resolves to, so everything recompiles.
- The per-symbol Mod table and tombstoned entries grow monotonically over a
  long interactive session (mirrors querylinger's written-objects note); object
  files of renamed functions also accumulate on disk.
- Integer division is unguarded (`sdiv`), matching the real compiler's LLVM
  backend: division by zero is a SIGFPE, where the DVM backend panics gracefully.
- Templates (`template{...}`) are not implemented — the Duckling frontend has no
  `template` keyword yet; only the baking-site expression `name:{args}` parses.
