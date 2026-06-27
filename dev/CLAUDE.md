# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Setup and build

- If `build/` exists, the repo is already initialized — do NOT run `./toolbox.py init` or `setup-build` again.
- First-time setup only: `./toolbox.py init` (submodules, venv, binaries), then `./toolbox.py setup-build` (requires LLVM 19).
- Always use `toolbox.py` wrappers, not raw cmake/ctest. Direct target build is fine: `cmake --build build --target <target> -j"$(nproc)"`.

## Commands

- Build + unit tests: `python3 toolbox.py test -b build -R <regex> -j"$(nproc)" --output-on-failure`
- Integration tests: `python3 toolbox.py itest -b build -t "integration_tests/<path>"` (framework docs: @integration_tests/README.md)
- Full PR validation (build, all tests, linters): `./toolbox.py pr-validate -b build --auto-fix` — pass clang-format/clang-tidy version 19 binaries via `-f`/`-t` (CI uses 19; newer versions flag checks CI doesn't). pr-validate may modify code.
- Lint only: `python3 toolbox.py cpp-linter -b build -f <clang-format-19> -t <clang-tidy-19> --auto-fix` and `python3 toolbox.py duck-linter`.
- `duck_add_test(<pack> <name> ...)` in CMake → binary `build/bin/tests/<name>_test`, ctest name `<name>_test`, source `tests/<name>_test.cpp`.

## Conventions

- Commit messages: `[Component] Action: description` — components like Compiler, DVM, REPL, DevOps, QuackPack; actions Add/Fix/Refactor/Change/Remove.
- C++23. clang-tidy enforces naming (warnings are errors): functions/methods `camelCase`, variables/members `snake_case`, constants `UPPER_CASE`, enums `PascalCase`.
- Column limit 100; indentation is tabs aligned with spaces (TabWidth 4) per `.clang-format`.
- TODOs must be `@TODO: #<issue_number> description` (todo-validate gates PRs), you may add an issue via `gh` and link it in the code.

## Structure notes

- `src/base/` utilities, `src/common/` cross-cutting (tokenizer, query framework, tester, diagnostics), `src/compiler/` (frontend, MIR/LIR, LLVM + DVM backends, formatter, REPL, LSP), `src/vm/` the DVM, `src/duck/` Rust CLI + QuackPack.
- `src/compiler/driver/driver/` nesting is intentional (driver + time_stats modules).
- Compilation is query-based and stateful (incremental), not traditional passes — be careful with query cache invalidation order.
