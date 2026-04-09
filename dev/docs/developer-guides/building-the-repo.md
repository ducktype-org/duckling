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


#### Debian/Ubuntu


Note that the dependencies listed below are listed without versions. Update the command accordingly, depending on package names on you system / you version preferences. 

```bash
sudo apt update -y && \
sudo apt install python3 python3-click doxygen graphviz-dev cmake ninja-build g++-14 lcov llvm-dev clang-tidy libzstd-dev zlib1g-dev -y
```


#### Arch linux

```bash
sudo pacman -S python python-pip python-click doxygen graphviz lcov --noconfirm
```

> **Note**  
> Some dependencies might be installed by default on your system, but if that's not the case, take a look at the list for Debian/Ubuntu.


#### MacOS

First of all, the toolbox requires at least Python3.12, while macOS default Python is Python3.9.
Secondly, there are two ways of installing clang on MacOS:
1. installing it from the Homebrew.
1. ~~using official Apple clang provided by Xcode,~~

~~You need to have at least Xcode 17.3 (clang version string `17.0.0`, run `clang --version` to check), so that it corresponds to the upstream clang 19.~~
~~You can check the mapping between Apple and LLVM versions [on the English Xcode Wikipedia page](https://en.wikipedia.org/wiki/Xcode#Toolchain_versions).~~

> [!IMPORTANT]
> Duckling requires Clang from LLVM/Homebrew in order to compile the project.
>
> Apple Clang is not yet supported.

```bash
# For macOS clang
# xcode-select --install

# For LLVM clang
brew install llvm@22
```

```bash
# Install remaining dependencies
brew install cmake ninja graphviz lcov doxygen python@3.12 clang-format llvm@19
```

> [!NOTE]
> You may have noticed, that there are two different versions of LLVM.
>
> This is intended, as LLVM 19 is required by the compiler to generate code,
> while LLVM 22 is required to provide clang 22 for compiling the project itself.

Also, unlike many Linuxes, Homebrew doesn't provide a lot of Python packages in their repositories.
Therefore, you have to create a local virtual environment and use it when running the toolbox.
```bash
cd dev/
# Verify you are running at least Python3.12
python3 --version
python3 -m venv .venv
source .venv/bin/activate
# Manually install Python dependencies
pip3 install click
pip3 install -r requirements.txt
```

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

CMake may not found LLVM installed from the Homebrew.
To prevent that set the LLVM directory **before** executing the above command.

```bash
export LLVM_DIR=${HOMEBREW_PREFIX}/opt/llvm@19
```

Also, ICU bundled with Apple Xcode doesn't provide the `<unicode/unistr.h>` header, therefore you are advised to install it with the Homebrew too.

```bash
brew install icu4c
```

As is the case with LLVM, CMake doesn't find ICU either.
Set the `$ICU_ROOT` variable **before** executing the `setup-build` toolbox command.

```bash
export ICU_ROOT=${HOMEBREW_PREFIX}/opt/icu4c
```

You also need to set clang 22 from LLVM as your main compiler.
This can be done in one of two ways:
1. Add LLVM 22 binaries to the path:
```bash
export PATH="${HOMEBREW_PREFIX}/opt/llvm@22/bin:${PATH}"
```
2. In `setup-build` prompts type full path to the clang 22 compiler (probably `/opt/homebrew/opt/llvm@22/bin/clang`).

> [!TIP]
> You may get some CMake errors about clang and LLVM versions mismatch.
> You can provide an extra argument to the `setup-build` command, `--clang-for-builtins <path-to-the-clang-19>` in order to work around them.
>
> It is important to use `clang-19` (from the `llvm@19` package, probably `/opt/homebrew/opt/llvm@19/bin/clang`).
> Otherwise you'll get LLVM errors in very late stages of the compiler.

> [!IMPORTANT]
> On macOS, only compiling the compiler (the `duckc` target) is supported.


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
There is also a CMake command to compile 
and run tests:

```bash
ninja build_all_tests          # to compile all tests
ninja test                     # to compile and run
ninja build_<test_suite>_tests # to compile a specific test suite
ninja test_<test_suite>        # to compile and run a specific test suite
```

You can compile a specific test. For example, if you want to run the `lexer_test_simple` test, 
you can run the following commands:

```bash
ninja lexer_test_simple
./bin/lexer_test_simple
```

> **Note**  
> You can also use the `ctest` command to run the tests, for example with regex name filter:  
> ```bash  
> ctest -R vm_  
> ```  
> This will run all tests that have `vm_` in their name at the beggining.


## Testing coverage

Before running this command make sure you build folder has enabled coverage.

```bash	
# This is only needed if build folder was not prepared for coverage.
./toolbox.py setup-build --coverage
./toolbox.py coverage
```
