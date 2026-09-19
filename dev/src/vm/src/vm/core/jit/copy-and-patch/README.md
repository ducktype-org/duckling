# Copy-and-Patch

The first, lightweight compiler is based on the novel Copy-and-Patch technique, described in first [this paper](https://fredrikbk.com/publications/copy-and-patch.pdf).

### Stencils

Stencils are binary code of micro-instruction implementations, that can be stitched together to create the compiled user code.

They are compiled by [`process_to_binary`](./CMakeLists.txt#11) cmake function and analyzed by [`analyze_bin.py`](../../../../../../../scripts/py/jit/analyze_bin.py).

### Patches

Patches are holes left by the compiler to fill in the relocations at a later time. We use them to fill in user-code-compile time constants.

Currently those are restricted to the exact values (eg. `local_stack`, `instr`, `arg0`), but in later development, we could include expressions (eg. `local_stack + arg0`).

## Importing stencils

Before compilation, stencils are dynamically loaded in order to resolve runtime addresses (eg. to VM and libc). This requires specific compilation flags (eg. `-mcmodel=large`), which prohibit optimizations, and fail to resolve error handling. Thus it should be replaced with a better solution.

## Compilation process

After allocating sufficient storage, all binary stencils corresponding to micro instructions in a basic block are copied and patched. At the end a special stencil is optionally added to ensure the correct control-flow. Non-jittable instructions are instead replaced with a special stencil as well.

## Inspecting the generated artifacts

When working on the stencil pipeline, the intermediate artifacts (the linked `-so` with stencil sections and the generated `stencils-cpp`) can be inspected with the LLVM tools the build scripts themselves use:

- `llvm-readobj --elf-output-style=JSON --expand-relocs --section-data --section-relocations --section-symbols --sections <binary>` — how [`analyze_bin.py`](../../../../../../../scripts/py/jit/analyze_bin.py) reads stencil sections and relocations.
- `llvm-nm --portability <bitcode>` — how [`jittable_interface.py`](../../../../../../../scripts/py/jit/jittable_interface.py) lists the opcode symbols.
- `llvm-cxxfilt` — demangling the section/symbol names back to C++ function signatures.

`analyze_bin.py` also accepts `--statistics` to dump per-stencil sizes and jump-removal results, and `-v` to echo the exact tool invocations.

