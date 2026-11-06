# Compiler CLI artifact interface

The `duckc` compilation commands share `-a` / `--artifact-location` for selecting the root
directory for build and debug artifacts. The default is `./duck_build/`. The commands also share
`--dump-ir` for writing compiler representations under `duck_debug_artifacts/` inside the selected
artifact directory, and `--print-ir` for printing them to stdout. Printing supports `hir`, `mir`,
`lir`, and `dbc`.

The dump categories are listed in compilation order:

| Category   | Extension | Backend |
|------------|-----------|---------|
| `hir`      | `.hir`    | Either  |
| `mir`      | `.mir`    | Either  |
| `lir`      | `.lir`    | Either  |
| `llvm`     | `.ll`     | LLVM    |
| `llvm-opt` | `.opt.ll` | LLVM    |
| `asm`      | `.s`      | LLVM    |
| `dbc`      | `.dbc`    | DVM     |

`llvm` contains the raw LLVM IR before LLVM optimization. `llvm-opt` contains IR after applying
the selected `-O` level and before code generation. They can be requested together, for example
`--dump-ir asm,llvm,llvm-opt`, without changing the assembly's optimization level. At -O 0 no
optimization passes run, so the .opt.ll file is the same as the .ll file.

The same artifact location and dump options apply to these commands:

| Command | Input | Compile-only behavior |
|---------|-------|-----------------------|
| `compile_module` | One `.dk` module | Always compiles to a backend object (`.o` for LLVM or `.dbc` for DVM). It has no final executable link step, so it does not need `--no-link`. |
| `compile_modules` | One or more loose `.dk` modules | `--no-link` leaves the per-module backend objects and skips intermediate library packaging and final executable linking. |
| `compile_package` | A package root or root module | `--no-link` leaves per-module backend objects and skips the final executable or library output, including when `--emit-static-lib` is selected. |
| `compile_packages` | A JSON package manifest | `--no-link` skips linking and archiving for `native`, `lib`, `dvm_exe`, and `dvm_lib` tasks, leaving their per-module backend objects. Manifest `obj` tasks retain their declared behavior. |
| `compile_script` | One `.dks` script | `--no-link` leaves the script's backend object (`.o` for LLVM or `.o.dbc` for DVM) and skips linking the final executable or DBC package. |

`--no-link` does not disable requested `--dump-ir` or `--print-ir` outputs. Compile-only commands
still report compilation failures normally. Use the command's `--help` output for its complete
command-specific options.

The focused CLI integration cases live in
[`integration_tests/compiler/cli/testconfig.yaml`](../../../integration_tests/compiler/cli/testconfig.yaml).
