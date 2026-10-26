# Compiler CLI artifact interface

The `duckc` compilation commands share `-a` / `--artifact-location` for selecting the root
directory for build and debug artifacts. The default is `./duck_build/`. The commands also share
`--dump-ir` for writing compiler representations under `duck_debug_artifacts/` inside the selected
artifact directory, and `--print-ir` for printing them to stdout. Printing supports `hir`, `mir`,
`lir`, and `dbc`.

The dump categories are listed in compilation order:

| Category | Extension | Backend |
|----------|-----------|---------|
| `hir`    | `.hir`    | Either |
| `mir`    | `.mir`    | Either |
| `lir`    | `.lir`    | Either |
| `llvm`   | `.ll`     | LLVM |
| `asm`    | `.s`      | LLVM |
| `dbc`    | `.dbc`    | DVM |

The same artifact location and dump options apply to these commands:

| Command | Input | Compile-only behavior |
|---------|-------|-----------------------|
| `compile_module` | One `.dk` module | Always compiles to a backend object (`.o` for LLVM or `.dbc` for DVM). It has no final executable link step, so it does not need `--no-executable`. |
| `compile_modules` | One or more loose `.dk` modules | `--no-executable` leaves the per-module backend objects and skips the final executable link. |
| `compile_package` | A package root or root module | `--no-executable` leaves per-module backend objects. If `--emit-static-lib` is also selected, the requested library target is still produced. |
| `compile_packages` | A JSON package manifest | `--no-executable` changes `native` and `dvm_exe` tasks to object-only tasks. Manifest `obj`, `lib`, and `dvm_lib` targets retain their declared behavior. |
| `compile_script` | One `.dks` script | `--no-executable` leaves the script's backend object (`.o` for LLVM or `.o.dbc` for DVM) and skips linking the final executable or DBC package. |

`--no-executable` does not disable requested `--dump-ir` outputs. Compile-only commands still report
compilation failures normally. Use the command's `--help` output for its complete command-specific
options.

@TODO: #3861 Requesting an applicable `--dump-ir` or `--print-ir` category currently bypasses cached
objects for all visited modules using that backend (including core and standard-library modules).
HIR, MIR, and LIR requests apply to both backends; LLVM IR and assembly apply only to LLVM, and DBC
only to DVM. Categories for the other backend leave its cached objects usable.

The focused CLI integration cases live in
[`integration_tests/compiler/cli/testconfig.yaml`](../../../integration_tests/compiler/cli/testconfig.yaml).
