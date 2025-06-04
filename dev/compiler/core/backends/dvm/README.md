# DVM Backend

## High level support overview

DVM backend translation layer currently supports:

* Primitive types
* Creating functions
* Every available opcode (this is true about the InstructionBuilder, but not the whole system, which is caused by the lack of those instructions in LIR (at least as of March 7th, 2025))
* Function calls
* Local variables of type `i64` only

DVM backend translation layer currently does **not** support:

* Unsigned arithmetic operations
* Tail calls
* Other types (besides primitives)
