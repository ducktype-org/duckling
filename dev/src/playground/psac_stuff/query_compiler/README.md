# Querylinger

A query-framework-based incremental compiler for the Psackling language subset,
built for a three-way comparison of incremental-compilation engines:

| compiler | front end + LLVM lowering | incremental engine |
|:---|:---|:---|
| `psacklinger` | shared (`psackling/pipeline`) | PSAC (self-adjusting computation, `psac::Mod` + `psac_propagate`) |
| `querylinger` | shared (`psackling/pipeline`) | query framework (`src/common/query_framework`, side inputs + `invalidateQueries`) |
| `duckc` | its own (production) | query framework (fine-grained PST side inputs) |

Because psacklinger and querylinger share the exact same parser, symbol tables and
per-function LLVM object emission, any behavior or performance difference between
them is attributable to the incremental engine alone.

## Architecture

The front end (parse → GlobalPass → work-item collection + hashing) runs outside
the graph on every compile, exactly like psacklinger — `pg::analyzeProgram`. The
backend is query-based:

- `QueryFunctionObject` (one per function, keyed by link name) lowers one function
  to an object file via the shared `pg::compileOneFunction`.
- `QueryGlobalsObject` emits the globals object (variable definitions, builtins,
  the synthetic initializer constructor).
- The driver (`ql::Compiler`) schedules all emission queries as entry points,
  awaits them, and links with the system clang. On `update()` it re-analyzes,
  diffs the side-input sets and calls `query::external::invalidateQueries` — the
  `psac_write` + `psac_propagate` analog. Only invalidated (or new) queries run.

### Dependency granularity

Where psacklinger's whole non-body world is two PSAC Mods (per-function body hash
+ one coarse globals hash), querylinger splits the globals Mod into per-symbol
side inputs, registered *at the use site* during lowering through the
`pg::SymbolUseObserver` hook:

| input | keyed by | registered when |
|:---|:---|:---|
| `FnBodyInput` | link name + body content hash | function emission starts |
| `ConstInput` | qualified name + value/type hash | lowering reads the constant |
| `SigInput` | link name + param/return type names | `getOrCreateFunction` (calls, self) |
| `ClassInput` | qualified name + field layout hash | struct type lowering |
| `GlobalVarInput` | link name + type name | `getOrCreateGlobal` |
| `GlobalDefsInput` | content hash of all var declarations | globals-object emission |
| `GlobalsInput` | imports + namespaces + module identity | every emission (coarse remainder) |

Consequences, all covered by tests (`tests/querylinger_test.cpp`):

- const edit → only its readers recompile (psacklinger: everything)
- global var initializer edit → zero recompiles, globals object + relink only
  (psacklinger: everything)
- parameter rename → only that function (signature inputs hash types, not names)
- return type change → the function and its (unchanged) callers, which then
  correctly report the type error
- class field added → only functions using the class
- function added/removed → no full-rebuild fallback; removed objects just leave
  the link line

Content hashes are SHA-256 (`base::Bit256`, via the `hashing` module) over
dprint output with source positions stripped (`stripPositions` in
`psackling/pipeline.cpp`), so edits don't invalidate functions merely shifted
by them, and the hash doubles as the collision-safe 256-bit node identity.

The emission queries are pure: they return object bytes, and the session writes
the changed ones to disk and links. The process-global query graph hosts many
sequential `ql::Compiler` sessions (the test suite creates hundreds) — each
session starts by invalidating all leftover input nodes, so it never shares
nodes with an unrelated program; only one session may be active at a time.

## Testing

`querylinger_test` instantiates the shared end-to-end corpus
(`psac_stuff/compiler_test_corpus/`, also run by `psacklinger_test`) plus
incremental tests asserting exact recompile counts:

```sh
cmake --build build --target querylinger_test -j"$(nproc)"
./build/bin/tests/querylinger_test
```

## Benchmarks

Cold compiles + thread scaling (three-way, hyperfine):

```sh
src/playground/psac_stuff/psac_compiler/bench_compiler.sh
```

Warm incremental update latency (psacklinger vs querylinger, interactive mode):

```sh
python3 src/playground/psac_stuff/psac_compiler/bench_incremental.py
```

Results land next to those scripts as `results_compiler_<name>.{json,md}` and
`results_incremental_<name>.md`.
