# Duckling Development Repository

Duckling is a C++23 programming language project with a comprehensive build system, testing framework, and documentation generation. The project uses CMake + Ninja for builds, Python toolbox for automation, and requires specific system dependencies.

**ALWAYS reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.**

## Working Effectively

### Bootstrap and Dependencies
- Install system dependencies:
  ```bash
  sudo apt update -y && sudo apt install python3 python3-click doxygen libgraphviz-dev cmake ninja-build g++-14 gcc-14 lcov llvm-19 llvm-19-dev clang-tidy-19 clang-format-19 libzstd-dev zlib1g-dev mold -y
  ```
- **CRITICAL**: Always use g++-14 and gcc-14 compilers. LLVM 19.1.0+ is required.
- Enter the development directory: `cd dev/`
- Initialize repository:
  ```bash
  python3 toolbox.py init
  ```
  - Downloads dependencies, sets up git submodules, creates Python virtual environment
  - **NOTE**: Binary downloads may fail in restricted environments - this is expected
- Setup build directory (fully non-interactive):
  ```bash
  yes '' | python3 toolbox.py setup-build \
    -b build \
    -s Ninja \
    -t Dev \
    -x g++-14 -c gcc-14 \
    --gcov-version gcov-14
  ```
  - The explicit flags cover all string/choice prompts (build directory, build system, build type, C/C++ compilers, gcov version).
  - The three remaining prompts (`Use ccache`, `Enable coverage`, `Enable JIT`) are boolean flags with no `--no-*` counterpart, so `yes ''` is piped in to accept their defaults (all disabled). To *enable* any of them, pass `--ccache`, `--coverage` or `--enable-jit` instead.
  - Takes ~10 seconds to configure

### Building the Project
- **Build with limited parallelism to avoid memory issues**:
  ```bash
  cd build && ninja -j2
  ```
  - **TIMING**: Build takes 10-20 seconds for incremental builds. NEVER CANCEL. Set timeout to 30+ minutes for safety.
  - **CRITICAL**: Use `-j2` flag to limit parallel jobs - unlimited parallelism causes linker to be killed due to memory constraints
- Alternative full build: `ninja all` (but use `-j2` if memory constrained)
- **Build artifacts location**: `build/bin/` directory contains compiled binaries

### Running Tests
- Run all tests:
  ```bash
  python3 toolbox.py test -b build
  ```
  - **TIMING**: Test suite takes 5-15 minutes. NEVER CANCEL. Set timeout to 30+ minutes.
- Run tests matching a label regex (e.g. only VM tests):
  ```bash
  python3 toolbox.py test -b build -L vm
  ```
  - Use `-L <regex>` to filter by CTest label. For example, if you changed VM code, run `-L vm` to test only the VM portion instead of the full suite.
- Run specific test suites:
  ```bash
  cd build
  ninja test                    # compile and run all tests
  ninja build_all_tests        # compile all tests only
  ninja <test_file_name>       # compile specific test
  ctest -R <pattern>           # run tests matching pattern
  ```

### Integration Tests
- Run integration tests:
  ```bash
  python3 toolbox.py itest -b build
  ```
  - **TIMING**: Integration tests take 10-20 minutes. NEVER CANCEL. Set timeout to 40+ minutes.

### Coverage Analysis
- Setup build with coverage enabled (fully non-interactive):
  ```bash
  yes '' | python3 toolbox.py setup-build \
    -b build-cov \
    -s Ninja \
    -t Dev \
    -x g++-14 -c gcc-14 \
    --gcov-version gcov-14 \
    --coverage
  ```
  - Passing `--coverage` skips its prompt; `yes ''` still handles `Use ccache` and `Enable JIT`.
- Run coverage:
  ```bash
  python3 toolbox.py coverage -b build-cov
  ```
  - **TIMING**: Coverage analysis takes 15-25 minutes. NEVER CANCEL. Set timeout to 45+ minutes.

### Documentation
- Build documentation:
  ```bash
  python3 toolbox.py docs -b build
  ```
  - Builds Sphinx and Doxygen documentation
  - The build directory passed to `-b` must have been configured with `--docs`, e.g. `setup-build ... --docs -b build-docs`.
  - **TIMING**: Documentation build takes 5-10 minutes. Set timeout to 20+ minutes.
- Direct CMake targets:
  ```bash
  cd build
  ninja docs                # build all docs
  ninja open-sphinx-docs    # build and open Sphinx docs
  ninja open-doxygen-docs   # build and open Doxygen docs
  ```

## Validation Scenarios

**ALWAYS test functionality after making changes by running these validation scenarios:**

### Basic Validation
1. **Build Test**: Ensure the project builds successfully:
   ```bash
   cd dev/build && ninja -j2
   ```
2. **Run VM Binary**: Test the main executable:
   ```bash
   cd dev/build && ./bin/VM --help
   ```
3. **Simple Test Run**: Run a subset of tests:
   ```bash
   cd dev && python3 toolbox.py test -b build
   ```

### Advanced Validation
1. **Integration Test**: Validate end-to-end functionality:
   ```bash
   cd dev && python3 toolbox.py itest -b build
   ```
2. **Documentation Generation**: Ensure docs build:
   ```bash
   cd dev && python3 toolbox.py docs -b build
   ```

## Common Tasks and Locations

### Key Directories
- `dev/` - Main development directory (ALWAYS work from here)
- `dev/src/` - Source code organized by component:
  - `dev/src/base/` - Base utilities and data structures
  - `dev/src/common/` - Common functionality
  - `dev/src/compiler/` - Compiler implementation
  - `dev/src/vm/` - Virtual machine implementation
- `dev/build/` - Build directory (created by setup-build)
- `dev/build/bin/` - Compiled executables
- `dev/docs/` - Documentation source
- `dev/integration_tests/` - Integration test framework
- `dev/scripts/` - Build and utility scripts

### Toolbox Commands Reference
```bash
python3 toolbox.py --help              # Show all available commands
python3 toolbox.py init                # Initialize repository (submodules, venv, binaries)
python3 toolbox.py setup-build         # Create build directory
python3 toolbox.py setup-venv          # Set up Python virtual environment only
python3 toolbox.py test                # Run unit tests
python3 toolbox.py itest               # Run integration tests
python3 toolbox.py coverage            # Run coverage analysis
python3 toolbox.py fix-coverage        # Fix stale coverage data issues
python3 toolbox.py docs                # Build documentation
python3 toolbox.py cpp-linter          # Run clang-tidy-19 and clang-format-19
python3 toolbox.py duck-linter         # Run custom C++ linting
python3 toolbox.py issue-checker       # Check for #issue_number references in source
python3 toolbox.py todo-counter        # Print counts of TODO comments in the code
python3 toolbox.py todo-validate       # Validate TODO comments in source files
python3 toolbox.py list-files          # List files in the repository
python3 toolbox.py run-preprocessor    # Run the C preprocessor on a file via CMake
python3 toolbox.py download-binaries   # Download necessary binary files (e.g. ccache)
python3 toolbox.py download-llvm       # Download a specific version of LLVM
python3 toolbox.py install-llvm        # Compile and install LLVM from source
python3 toolbox.py pr-validate         # Validate branch for PR
python3 toolbox.py clean-init          # Clean initialization artifacts
```

> **IMPORTANT — non-interactive usage**: Most toolbox commands prompt for input (e.g. build directory, compilers, build type, linter paths).
> When running these commands, **always pass the required values as CLI flags** so the command does not hang waiting for interactive input.
> Run `python3 toolbox.py <command> --help` to see available flags for any command.
> Commands with **no** prompts (safe to run as-is): `init`, `setup-venv`, `duck-linter`, `issue-checker`, `todo-validate`, `todo-counter`, `list-files`, `download-binaries`.

### Pre-commit Validation
**ALWAYS run these commands before committing changes:**
```bash
cd dev
python3 toolbox.py cpp-linter -b build -f clang-format-19 -t clang-tidy-19 --auto-fix   # C++ linting and formatting
python3 toolbox.py duck-linter --auto-fix                                                # Custom linting rules
python3 toolbox.py test -b build                                                         # Run tests
```

### PR Validation
**Use `pr-validate` for comprehensive validation before creating a pull request:**
```bash
cd dev
python3 toolbox.py pr-validate \
  -b build \
  -f /usr/bin/clang-format-19 \
  -t /usr/bin/clang-tidy-19 \
  --auto-fix
```
- **What it does**: Runs a complete validation suite including build, tests, linting, duck-linter, todo-validate, and issue-checker
- **When to use**: Before creating a pull request to ensure all checks pass
- **`--auto-fix`**: Automatically applies clang-format and clang-tidy fixes — **highly recommended**. Use `--no-fix` to only report issues without changing files.
- **Tool paths**: `-f` sets the clang-format path and `-t` sets the clang-tidy path. The paths above (`/usr/bin/clang-format-19`, `/usr/bin/clang-tidy-19`) are typical on Ubuntu with `llvm-19` installed, but may differ on your system (e.g. `/usr/lib/llvm-19/bin/clang-format`). Run `which clang-format-19` to find the correct path.
- **`-b`**: Path to the build directory containing `compile_commands.json` (e.g. `build`).
- **TIMING**: Takes 20-30 minutes to complete all validations. NEVER CANCEL. Set timeout to 45+ minutes.

### CI Workflow Compatibility
- The project uses GitHub Actions with 40-minute timeout per matrix run
- Self-hosted runners use 6 threads for Dev builds, 3 for DevOpt builds
- GitHub runners use 2 threads
- CI validates: build, test, integration tests, linting, documentation
- On PRs, only Dev/gcc runs by default; full matrix (Dev+DevOpt, gcc+clang) runs on main/dev branches, non-PR events, PR labeled "Run All Workflows", or PR with an approved review

## Memory and Performance Notes

- **CRITICAL**: Always use `ninja -j2` for builds to avoid memory exhaustion
- Large linking operations may require limiting parallelism further
- VM and compiler components are memory-intensive during linking
- Integration tests exercise the full compiler pipeline

## Troubleshooting

### Build Issues
- **Linker killed (signal 9)**: Use `ninja -j1` or `ninja -j2` to reduce memory usage
- **LLVM not found**: Ensure `llvm-19-dev` is installed: `sudo apt install llvm-19 llvm-19-dev`
- **Compiler errors**: Verify g++-14 is installed and specified in setup-build

### Test Issues
- **Tests timing out**: Increase timeout, tests can take 15+ minutes
- **Integration tests failing**: Ensure build completed successfully first
- **Coverage missing**: Re-run setup-build with `--coverage` flag

### Documentation Issues
- **Sphinx errors**: Check Python virtual environment is active
- **Missing files**: Run `ninja clean` and reconfigure CMake
- **Doxygen missing**: Install with `sudo apt install doxygen`

## Development Workflow
1. Always start in `dev/` directory
2. Use toolbox commands for all major operations
3. Build with `ninja -j2` to avoid memory issues
4. Test changes with appropriate validation scenarios
5. Run linting before committing
6. Allow sufficient time for builds and tests (never cancel long-running operations)