# Duckling Development Repository

Duckling is a C++23 programming language project with a comprehensive build system, testing framework, and documentation generation. The project uses CMake + Ninja for builds, Python toolbox for automation, and requires specific system dependencies.

**ALWAYS reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.**

## Working Effectively

### Bootstrap and Dependencies
- Install system dependencies:
  ```bash
  sudo apt update -y && sudo apt install python3 python3-click doxygen graphviz-dev cmake ninja-build g++-14 gcc-14 lcov llvm-19 llvm-19-dev clang-tidy-19 clang-format-19 libzstd-dev zlib1g-dev -y
  ```
- **CRITICAL**: Always use g++-14 and gcc-14 compilers. LLVM 19.1.0+ is required.
- Enter the development directory: `cd dev/`
- Initialize repository:
  ```bash
  python3 toolbox.py init
  ```
  - Downloads dependencies, sets up git submodules, creates Python virtual environment
  - **NOTE**: Binary downloads may fail in restricted environments - this is expected
- Setup build directory:
  ```bash
  python3 toolbox.py setup-build -x g++-14 -c gcc-14 --gcov-version gcov-14
  ```
  - Press ENTER for all prompts to use defaults
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
  python3 toolbox.py test
  ```
  - **TIMING**: Test suite takes 5-15 minutes. NEVER CANCEL. Set timeout to 30+ minutes.
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
  python3 toolbox.py itest
  ```
  - **TIMING**: Integration tests take 10-20 minutes. NEVER CANCEL. Set timeout to 40+ minutes.

### Coverage Analysis
- Setup build with coverage enabled:
  ```bash
  python3 toolbox.py setup-build --coverage -x g++-14 -c gcc-14
  ```
- Run coverage:
  ```bash
  python3 toolbox.py coverage
  ```
  - **TIMING**: Coverage analysis takes 15-25 minutes. NEVER CANCEL. Set timeout to 45+ minutes.

### Documentation
- Build documentation:
  ```bash
  python3 toolbox.py docs
  ```
  - Builds Sphinx and Doxygen documentation
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
   cd dev && python3 toolbox.py test
   # Let it prompt for build directory, press ENTER for defaults
   ```

### Advanced Validation
1. **Integration Test**: Validate end-to-end functionality:
   ```bash
   cd dev && python3 toolbox.py itest
   ```
2. **Documentation Generation**: Ensure docs build:
   ```bash
   cd dev && python3 toolbox.py docs
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
python3 toolbox.py init                # Initialize repository
python3 toolbox.py setup-build         # Create build directory
python3 toolbox.py test                # Run unit tests
python3 toolbox.py itest               # Run integration tests
python3 toolbox.py coverage            # Run coverage analysis
python3 toolbox.py docs                # Build documentation
python3 toolbox.py cpp-linter           # Run clang-tidy-19 and clang-format-19
python3 toolbox.py duck-linter          # Run custom C++ linting
python3 toolbox.py pr-validate          # Validate branch for PR
python3 toolbox.py clean-init           # Clean initialization artifacts
```

### Pre-commit Validation
**ALWAYS run these commands before committing changes:**
```bash
cd dev
python3 toolbox.py cpp-linter    # C++ linting and formatting with clang-tidy-19 and clang-format-19
python3 toolbox.py duck-linter   # Custom linting rules
python3 toolbox.py test          # Run tests
```

### PR Validation
**Use `pr-validate` for comprehensive validation before creating a pull request:**
```bash
cd dev
python3 toolbox.py pr-validate   # Comprehensive PR validation
```
- **What it does**: Runs a complete validation suite including build, tests, linting, duck-linter, and issue-checker
- **When to use**: Before creating a pull request to ensure all checks pass
- **Options**: Can specify custom clang-tidy-19 and clang-format-19 paths using `-t` and `-f` flags
- **TIMING**: Takes 20-30 minutes to complete all validations. NEVER CANCEL. Set timeout to 45+ minutes.

### CI Workflow Compatibility
- The project uses GitHub Actions with 25-minute timeout for build+test
- Self-hosted runners use 6 threads for Dev builds, 3 for DevOpt builds
- GitHub runners use 2 threads
- CI validates: build, test, integration tests, linting, documentation

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