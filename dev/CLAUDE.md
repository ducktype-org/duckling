# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Setup and build

- If `build/` exists, the repo is already initialized — do NOT run `./toolbox.py init`, `install-llvm`, or `setup-build` again.
- First-time setup only (from `dev/`): `./toolbox.py init` (submodules, venv, binaries, deps), then `./toolbox.py setup-build` (press enter on prompts for defaults). Requires LLVM 19 and g++ 14+ or clang++ 19+. If the build fails on LLVM, `./toolbox.py install-llvm` fetches `19.1.7` locally into `scripts/downloads`.
- The default build system is **Ninja**. Compiled binaries land in `build/bin/`; compiled test binaries land in `build/bin/tests/`.
- Use `-jN` for all builds and parallel test runs, where N is the number of threads.

## Building — prefer direct ninja targets

Work from inside `build/`. Build only what you need with a concrete target instead of `ninja all`.

```bash
ninja -j10 <target>        # e.g. ninja -j10 helios_test
ninja -j10 duckc           # the compiler binary
ninja -j10 VM              # the DVM binary
```

- A module test named `foo_test` builds with `ninja -j10 foo_test` and runs as `./bin/tests/foo_test`.
- Full CMake docs on targets: docs/developer-guides/building-the-repo.md and docs/developer-guides/writing-tests-examples.md.

## Testing

### While iterating — run the concrete test

Build and run the single relevant test binary directly. It is the fastest loop.

```bash
ninja -j10 helios_test
./bin/tests/helios_test
```

Our test binaries take standard filters — run a subset when the binary is large rather than the whole thing.

### Final verification — build the bundle, then ctest by label

Before considering a change done, build the relevant test bundle and run the matching ctest label:

```bash
# Compiler changes:
ninja -j10 build_compiler_tests
ctest -L compiler -j10

# VM / DVM changes:
ninja -j10 build_vm_tests
ctest -L vm -j10
```

Available bundle targets: `build_base_tests`, `build_common_tests`, `build_compiler_tests`, `build_vm_tests`, `build_all_tests`.
Available ctest labels (`-L`): `base`, `common`, `compiler`, `vm`.
`ctest -R <regex>` filters by test name (e.g. `ctest -R vm_ -j10`).

### Toolbox alternatives

- `python3 toolbox.py test -b build -R <regex> -j10 --output-on-failure` — build + run.
- `python3 toolbox.py itest -b build -t "integration_tests/<path>"` — integration tests (framework docs: integration_tests/README.md).

## Investigating bugs and problems

When hunting a bug, drive the compiler against a concrete example and inspect its intermediate output. Binaries are in `build/bin/`.

### Compile an example package

```bash
./bin/duckc compile_package -n main "<path_to_test>"
```

Run `./bin/duckc compile_package --help` for the flag list.

Example — dump MIR and HIR for a failing case:

```bash
./bin/duckc compile_package -n main "integration_tests/scripts/basic_tests/simple_addition/simple_addition.ds" --print-ir mir,hir
```

### Developer logs

Enable category-scoped logs with the global `--dev-logs <categories>` flag (comma-separated):

```bash
./bin/duckc --dev-logs Compiler,Query,Incremental compile_package -n main "<path>"
```

Categories are enumerated in `src/common/logger/src/logger/logger.hpp`. Enable `QueryStacktraces` / `NYIStacktraces` to get stacktraces on query errors and not-yet-implemented hits.

## Linting and PR validation

- Full gate (build, all tests, linters): `./toolbox.py pr-validate -b build --auto-fix`. Pass clang-format/clang-tidy **version 19** binaries via `-f`/`-t` — CI uses 19; newer versions flag checks CI does not. This may modify code.
- Lint only: `python3 toolbox.py cpp-linter -b build -f <clang-format-19> -t <clang-tidy-19> --auto-fix` and `python3 toolbox.py duck-linter`.
- Format only: `./scripts/formatting/format_repo_cpp.sh` (changed files) or `./scripts/formatting/format_repo_cpp_all.sh` (all).

## Conventions

- Do not add unnecessary comments, it's often our convention to write the code like this and it is pretty visible to someone who has seen the code before the change, in such cases do not add additional comments explainin why the new lines there when added
- **C++23.** clang-tidy enforces naming (warnings are errors): functions/methods `camelCase`, variables/members `snake_case`, constants `UPPER_CASE`, enums `PascalCase`.
- TODOs must be `@TODO: #<issue_number> description` (todo-validate gates PRs). Open an issue via `gh` and link it if needed.
- Commit / PR titles: `[Area] (Ft. Feature) Character: short description (#PR)`. Areas: `Compiler`, `DVM`, `QuackPack`, `Duck`, `Base`, `GC`, `Docs`, `DevOps`, `[<common-submodule>]`. Characters: `Add`, `Fix`, `HotFix`, `Refactor`, `Delete`, `Update`, `Maintenance`, `Change`. Feature is optional. `squash and merge`; feature-branch commits may be meaningless. Full spec: docs/developer-guides/how-to-commit.md.
- Add a test with `duck_add_test(<pack> <name> <source> USES <modules...>)` in CMake → binary `build/bin/tests/<name>`, ctest name `<name>`. Examples use `duck_add_example(...)`.

## Structure notes

- `src/compiler/driver/driver/` nesting is intentional (driver + time_stats modules).
- Compilation is **query-based and stateful (incremental)**, not traditional passes — be careful with query cache invalidation order. Use `--no-incremental` when a bug might be a stale-cache artifact, and `--print-graph` to inspect the query graph.
