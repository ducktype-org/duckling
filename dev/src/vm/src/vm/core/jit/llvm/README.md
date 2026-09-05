# The LLVM JIT compiler

The optimizing JIT compiler is based on the LLVM ORC API. For that purpose we generate the LLVM IR module that corresponds to a list of calls to micro instructions. We link that module with the instruction implementations, and run optimization passes. This process has a few notable complexities.

### Absolute symbols

To support unjitable instructions, they are introduced as "Absolute symbols" to the LLVM compiler, meaning that those functions (instruction implementations) have constant address, in the running VM process.

### Constant instructions

To allow for better optimizations, the (compile-time) information about arguments of successive instructions have to be passed to the compiler. We achieve this by creating a constant list of instructions, and pointing the instruction pointer to it. This list should be largely optimized away, apart from some exceptions (eg. unjitable).

### Returning the `*instr`

Since the instruction pointer was moved to the aforementioned list, it has to be reset. This happens naturally, in the case of functions thanks to the last instruction being the `ret` (or `ret_tailcall_func`). In the case of loops, the offset from the original instruction pointer (which is stored in the CFG) is returned. 

## Importing bitcode

Importing the instruction implementations' bitcode happens in [opcodes_bitcode_source.cpp](./opcodes_bitcode_source.cpp). It parses the compiled bitcode file as an LLVM module. This is the main reason for the increase in startup time. 

During compilation, the bitcode corresponding to the used instructions is then copied over to a new module, to avoid the disappearing module issue, and reduce compilation time.
