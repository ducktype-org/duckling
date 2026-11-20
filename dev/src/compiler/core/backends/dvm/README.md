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

## Incremental lowering (REPL, compile-time evaluation)

The REPL and the compile-time evaluator use `IncrementalDVMCodeBuilder` (DVM backend) to keep a
persistent lowering state across statements / evaluations. The context wraps `ProgramLoweringContext` and exposes an incremental API:

- `insertLIRUnitAndCollectNewlyLoweredCode(lir_unit)`

This allows REPL/script execution to load only new code into the VM instead of rebuilding the
entire bytecode program on each statement.

`IncrementalDVMCodeBuilder` is intended to be owned by a single REPL session (or compile-time
evaluator) and used from a single threaded compilation flow; sharing the context across concurrent sessions is not supported.
