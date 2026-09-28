# Building the project

## Before start

### Installing dependencies

- **Python3** and **click** library are required for the toolbox script.
- **Doxygen** is used for generating documentation.
- **Graphviz** is dependency used for generating diagrams by the compiler.
- **CMake** and **Ninja** are used for building the project.
- **g++** with version 14 or higher is required for building the project.
- [alternatively to g++] **clang++** with version 19 or higher is required for building the project.
- **lcov** is used for generating coverage reports.
- **LLVM** with version 19 is required for building the project.
- **pkg-config** and **libffi** (development headers) are required for the dynamic foreign function interface.
- [optional] **libclang** (development headers, same version as LLVM) builds `duck_c_import`, the translator behind `duck translate-c`. Without it the tool and its tests are skipped. It ships with LLVM from `toolbox.py download-llvm`/`install-llvm` and with Arch's `clang`; on Debian/Ubuntu install `libclang-dev`.


#### Debian/Ubuntu


Note that the dependencies listed below are listed without versions. Update the command accordingly, depending on package names on you system / you version preferences. 

```bash
sudo apt update -y && \
sudo apt install python3 python3-click doxygen graphviz-dev cmake ninja-build g++-14 lcov llvm-dev clang-tidy libzstd-dev zlib1g-dev pkg-config libffi-dev libclang-dev -y
```


#### Arch linux

```bash
sudo pacman -S python python-pip gcc clang lld mold python-click python-requests python-yaml ninja cmake doxygen graphviz lcov pkgconf libffi --noconfirm
```

> **Note**  
> Some dependencies might be installed by default on your system, but if that's not the case, take a look at the list for Debian/Ubuntu.


#### MacOS (gcc)

The project is designed around **libstdc++** (for example it links `-lstdc++exp` for `<stacktrace>`),
so on macOS it is built with **Homebrew GCC**, not Apple clang or Homebrew's clang (those use
libc++, whose ABI is incompatible). The supported toolchain is `g++-15` + libstdc++ together with a
copy of LLVM 19 built from source with the same compiler (see the [install-llvm](#optional-but-recommended-installing-custom-llvm-library)
step below), so its libraries share the libstdc++ ABI. Xcode's command line tools are still needed
for the macOS SDK and the system linker (`ld64`).

```bash
xcode-select --install

# GCC (provides g++-15 / gcc-15), the build tools, ICU and libffi.
brew install gcc@15 cmake ninja graphviz lcov doxygen python pkg-config libffi icu4c

# clang-format / clang-tidy 19 are only used for linting (pr-validate / cpp-linter).
brew install llvm@19
```

Pass GCC explicitly to the toolbox commands that build — `-x g++-15 -c gcc-15` on `install-llvm` and
`setup-build` — otherwise the toolbox picks up Apple clang.

The toolbox requires at least Python 3.12 (the macOS system Python is older; any newer Homebrew
Python works), and Homebrew doesn't package most of the Python dependencies. Create a local virtual
environment, activate it, and run the toolbox from it:
```bash
cd dev/
python3 -m venv .venv
source .venv/bin/activate
pip3 install click -r requirements.txt
```

#### MacOS (clang)

If you want to use the clang instead of the **Homebrew GCC**, it's perfectly fine, but you don't get the stack-traces on errors. 
The minimal clang version that supports the compilation is the 23 from the LLVM 23. 
But we still need LLVM 19, if we want to use a system-wide precompiled LLVM version
and for the `clang-format` and `clang-tidy`.
You can install it using:

```bash
brew install llvm@23
brew install llvm@19
# The linker. Homebrew keeps it in its own formula, so neither llvm above provides one.
brew install lld
```

Installing using Homebrew doesn't expose the aliases visible from the terminal session 
(not to override the system default `clang`), you can pass to the `setup-build` the paths directly 
or create some aliases. 

When compiling the repo with clang, we don't have to compile the LLVM manually, but we can use the 
compiled version downloaded from Homebrew.
To make the LLVM@19 visible from the CMake
in our project add this export:

```bash
export CMAKE_PREFIX_PATH="/opt/homebrew/opt/llvm@19${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
```

lld supports Mach-O these days, and `setup-build` picks it on macOS on its own - see the linker note
below. Pass `--linker default` to go back to Apple's `ld`.

In order to run toolbox the requirements are the same as for MacOS (gcc).

## Toolbox

The toolbox script is a Python script that helps with the initialization of the repository,
building the project, running tests, and other tasks.
It is located in the `dev` directory of the project.

To run the script, when you are in the `dev` directory, you can use `./toolbox.py <command>` or `python3 toolbox.py <command>`.

To see the available commands you can run the srcipt without any arguments:

```bash
./toolbox.py
```

Similarly, you can get help for a specific command:

```bash
./toolbox.py <command> --help
```

Below are the most common commands that you will use when building the project.

> **Tip**  
> If you want a more in-depth look at the inner workings of the build system
> you can look at the Python code that makes up the `toolbox.py` script.


## Setting up the repo

To begin, enter the `dev/` directory and then:


### Initialize the repository with toolbox

```bash
./toolbox.py init
```

This:

- fetches library dependencies
- updates git submodules
- setups virtual environment (important for building docs)
- downloads binaries, etc...


### [Optional, but recommended] Installing custom LLVM library

```bash	
./toolbox.py install-llvm
```

In case of trouble during build or `setup-build` step, it is advised
to install the latest supported version of LLVM for your system 
(the required dependencies are already listed in the dependencies list above).
But the most reliable way is to download the LLVM locally using toolbox with this command.

After using this command, LLVM is **NOT installed system-wide**, but only for this project.
Using version `19.1.7` should work for most platforms.

The installed library is placed in the `scripts/downloads` directory.


### Create a build folder

```bash
./toolbox.py setup-build
```

Press "enter" on every prompt to leave default options. The setup now automatically:
- Detects and uses the best available linker (mold/lld if available)
- Infers GCOV version from your compiler version
- Disables documentation building by default (use `--docs` flag to enable)

Advanced options like unity compilation, LTO, and symbol stripping are available as command-line flags only (use `--help` to see all options).


#### MacOS caveats

On macOS the `install-llvm` step above is **required** (Homebrew's `llvm@19` is built against libc++,
which is ABI-incompatible with the libstdc++ this project uses). Run it — and `setup-build` — with GCC:

```bash
./toolbox.py install-llvm -x g++-15 -c gcc-15
```

The ICU bundled with the macOS SDK is a C-only subset, so CMake must be pointed at the Homebrew ICU
(installed above) with `$ICU_ROOT` **before** running `setup-build`:

```bash
export ICU_ROOT=${HOMEBREW_PREFIX}/opt/icu4c
```

`setup-build` auto-selects `lld` on macOS, and falls back to the system linker when it is missing.
mold is never a candidate there: it has no Mach-O backend, only ELF. lld does (`ld64.lld`), and on
the CI mac it links measurably faster than Apple's `ld` - 0.43s against 0.63s for the common test
pack - while not emitting the duplicate-library warnings `ld` prints. Install it with
`brew install lld`; Homebrew keeps it in its own formula, so `llvm@23` does not bring one.


## Compiling the project

Ninja and Unix Makefiles are two alternative build systems that can be used to compile the project.
You can choose which you want to use in the `toolbox.py` when creating a build folder.
The default option is the Ninja build system, as it uses all available cores to compile the project by default.
Ninja works the same way as Unix Makefiles, so any command, like `ninja <target>` can be replaced with `make <target>`.

After initialization, you can compile the project by running in the build directory:

```bash
ninja <target>
ninja all
```

> **Important**  
> The compiled binaries are inside the ``build/bin`` directory.


## Building the docs

If you want to build the docs, make sure to enable the `--docs` flag when creating the build directory with `setup-build` command (e.g., `./toolbox.py setup-build --docs`).
For building the documentation, the Python virtual environment created in the `init` step is used.

```bash
./toolbox.py docs
```

This builds the docs and opens them in your favorite browser. 
Leave defaults if you chose defaults in previous step.


### Using CMake targets

The more direct way to compile the documentation is to use the CMake targets.
After you have created the build directory,
to compile the documentation (source-doc and doxygen) you can run the build target:

```bash
ninja docs
```

This will compile the documentation and put it in the `build/docs` subdirectory.
There is also a custom target for opening the documentation in the browser:

```bash
ninja open-doxygen-docs
```

> **Note**  
> Sometimes when creating new files in the documentation, Sphinx might not recognize them.  
> To fix this, you can run the `ninja clean` command or rerun the CMake configuration.


### Docs configuration

Our Sphinx documentation configuration is in the `docs/doc-config` directory.
It is currently a separate Github repository that is included as a submodule in the main repository.
When doing changes to the configuration, you should commit them to the submodule repository and then update 
the main repository with the new submodule commit.


## Running the tests

To compile and run tests, you can use the toolbox script:

```bash	
./toolbox.py test     # regular tests
./toolbox.py test -m  # run tests under valgrind
```

It's useful to be aware of more direct methods for running the tests.
We use the CTest tool from CMake to manage the test files. 
There is also a CMake command to compile tests:

```bash
ninja build_all_tests          # to compile all tests
ninja build_<test_suite>_tests # to compile a specific test suite
```

You can compile a specific test. For example, if you want to run the `lexer_test_simple` test, 
you can run the following commands:

```bash
ninja lexer_test_simple
./bin/lexer_test_simple
```

It's often useful to run a bundle of tests, for example all the tests in the `compiler` test bundle, to do that
you can use `-L` (label filter) on `ctest`:

```bash
ctest -L compiler
```

> **Note**  
> You can also use the `ctest` command to run the tests with other configurations:  
> ```bash  
> ctest -R vm_  -j 4
> ```  
> This will run all tests with 4 threads that have `vm_` in their name at the beginning.


## Testing coverage

Before running this command make sure you build folder has enabled coverage.

```bash	
# This is only needed if build folder was not prepared for coverage.
./toolbox.py setup-build --coverage
./toolbox.py coverage
```
