# VM Tests

* [performance](./performance/readme.md)

## Layout

Tests are grouped by what they check. Each suite keeps its `.dbc` fixtures next to its source,
and starts with a comment saying what should be tested in it.

| Directory | What lives there |
|---|---|
| `common/` | `VmTestSuite` / `VmRuntimeTestSuite` / `VmLoaderTestSuite` helpers (`vm_tester_utils.{hpp,cpp}`). |
| `common/programs/` | Fixtures shared by several suites (`while_true`, `spin_threads`, `breakpoint`, leaks, `fib_*`, ...). |
| `utils/` | VM data structures that need no program (`persistent_structures`, `stable_obj_id_name_map`). |
| `bytecode/` | Instruction naming, program builders (`builders/`), the assembly parser (`assembly/`, syntax errors only), the serializer (`serializer/`). |
| `loader/verification/` | Static verification: every semantic error found before a program runs, C pointer instructions included (written inline with `inlineDbc`). Programs that must load go in `right/`, programs that must fail go in `wrong/<area>/`. |
| `loader/lowering/` | Load-time lowering (GIL placement, local initialization, inheritance metadata and vtables). |
| `loader/inheritance/` | Every hierarchy / virtual-call error found at load time. |
| `loader/ffi/` | Every FFI error found at load time: `ffi object` files that cannot be loaded, missing symbols, signatures libffi cannot describe. Programs are inline (`inlineDbc`), since they name `ffi_test_lib` by its build path. |
| `runtime/primitives/` | Integers, booleans, literals, casts, `void`. |
| `runtime/control_flow/` | Jumps and labels inside one function. |
| `runtime/functions/` | Calls, arguments, return values, recursion, builtins, `main` (exit code, arguments). |
| `runtime/globals/` | Global variables: initial values, constructors, destructors. |
| `runtime/pointers/` | Pointers to locals, globals and heap blocks, linked lists. |
| `runtime/structures/` | Structures and nested aggregates. |
| `runtime/arrays/` | `fixed_size_table/` and `dynamic_table/`. |
| `runtime/variants/` | Variants and their type tag. |
| `runtime/inheritance/` | Virtual dispatch, casts, setting vtables (`setting_vtable/`). |
| `runtime/memory/` | Memory management that must work: destructors, variant blocks. |
| `runtime/threads/` | Threads, mutexes, condition variables. |
| `runtime/correctness/` | Whole algorithms with a known result. |
| `runtime/native/` | `extern_c/` (C++ functions registered in the VM) and `ffi/` (calls into shared libraries, `cptr` to native memory). `ffi/` also builds `ffi_test_lib`. Programs are inline (`inlineDbc`), since they name the library by its build path. |
| `runtime/errors/` | Every error the VM detects while a program runs (zero division, leaks, double / invalid free, use after free, null pointer or cpointer, out of bounds, unset vtable, ...). Fixtures in one folder per area. `threads/` holds the errors that need several threads (deadlocks included) in a second binary. |
| `api/` | `vm::api` endpoints: process lifecycle, stress, `debug/` (debug endpoints), `injection/` (code injection) and `run_function/` (running one function: arguments, return values). |
| `debugger/` | The `vm::debugger` tool itself (debugger class, CLI, mapper). The CLI helpers and the mapper run no program, so they run once, against the SC build. |
| `jit/`, `fast_mode/` | The JIT and the fast execution mode. |

### Which suite gets a new test?

* A program that must be **rejected at load time** (or must load fine without being run) belongs in
  a `loader/` suite. Those suites run once, against the SC build only. A pure syntax error belongs
  in `bytecode/assembly/`.
* A program that loads but must be **stopped by the VM while it runs** belongs in `runtime/errors/`
  (or `runtime/errors/threads/` if it needs several threads).
* Suites under `runtime/` derive from `VmRuntimeTestSuite`, which deletes `loadInvalidDbc`,
  `loadThenLoadInvalidDbc` and `loadValidDbc`. A loader check there does not compile.
* Suites under `loader/` that load through `vm::api` derive from `VmLoaderTestSuite`, which deletes `runTestOnVm`,
  `runTestOnVmGetResult`, `runFunctionSynchronouslyAsTest` and `runFunctionExpectPanic`. Running
  a program there does not compile, it belongs in a `runtime/` suite.
* Suites registered in `duck_add_vm_tests` run twice, once per VM flavour (`vm_sc_*` and `vm_tc_*`).
* A suite that stops or pauses programs cannot run with the JIT (#3612). It either spawns its
  processes with `process_config.enable_jit = false` (like `api_test`), or is disabled in JIT
  builds with `duck_disable_when_jit` in `CMakeLists.txt`.

## Unit tests

To compile all VM tests:
```
ninja -j$(nproc) build_vm_tests
```

To run one test from build directory:
```
ctest -R vm_sc_primitives_test
```

To run all VM tests from the build directory and see the error output:
```
ctest -L vm -j$(nproc) --output-on-failure
```

Alternatively, you can run the tests from the `dev/build` directory:
```
./bin/tests/vm_sc_primitives_test
```
