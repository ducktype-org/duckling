# VM Tests

* [performance](./performance/readme.md)

## Layout

Tests are grouped by what they check. Each suite keeps its `.dbc` fixtures next to its source.

| Directory | What lives there |
|---|---|
| `common/` | `VmTestSuite` / `VmRuntimeTestSuite` helpers (`vm_tester_utils.{hpp,cpp}`). |
| `common/programs/` | Fixtures shared by several suites (`while_true`, `spin_threads`, `breakpoint`, leaks, `fib_*`, ...). |
| `utils/` | VM data structures that need no program (`persistent_structures`, `stable_obj_id_name_map`). |
| `bytecode/` | Instruction naming, program builders (`builders/`), the assembly parser (`assembly/`), the serializer (`serializer/`). |
| `loader/verification/` | Static verification. Programs that must load go in `right/`, programs that must fail go in `wrong/<area>/`. |
| `loader/lowering/` | Load-time lowering passes (GIL placement, local initialization). |
| `loader/inheritance/` | Inheritance metadata and every hierarchy / virtual-call error found at load time. |
| `runtime/` | What a program does when it runs: `basic/`, `functions/`, `pointers/`, `aggregates/{fixed_size_table,dynamic_table}/`, `variant/`, `inheritance/`, `memory/`, `correctness/`, `threads/` (+ `deadlock_detection/`), `native/{extern_c,ffi}/`. |
| `api/` | `vm::api` endpoints: the smoke test, process lifecycle, stress, `debug/` (debug endpoints) and `injection/` (code injection). |
| `debugger/` | The `vm::debugger` tool itself (debugger class, CLI, mapper). |
| `jit/`, `fast_mode/` | The JIT and the fast execution mode. |

### Which suite gets a new test?

* A program that must be **rejected at load time** (or must load fine without being run) belongs in
  a `loader/` suite. Those suites run once, against the SC build only.
* Suites under `runtime/` derive from `VmRuntimeTestSuite`, which deletes `loadInvalidDbc`,
  `loadThenLoadInvalidDbc` and `loadValidDbc`. A loader check there does not compile.
* Suites registered in `duck_add_vm_tests` run twice, once per VM flavour (`vm_sc_*` and `vm_tc_*`).

## Unit tests

To compile all VM tests:
```
ninja -j$(nproc) build_vm_tests
```

To run one test from build directory:
```
ctest -R vm_sc_micro_test
```

To run all VM tests from the build directory and see the error output:
```
ctest -L vm -j$(nproc) --output-on-failure
```

Alternatively, you can run the tests from the `dev/build` directory:
```
./bin/tests/vm_sc_micro_test
```
