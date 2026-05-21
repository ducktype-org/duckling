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

## REPL lowering

The REPL uses `ReplLoweringContext` (DVM backend) to keep a persistent lowering state across
statements. The context wraps `ProgramLoweringContext` and exposes an incremental API:

- `captureLoweredEntitiesSnapshot()` records how many types/globals/functions have been lowered.
- `collectNewCodeSince(snapshot)` returns only the newly-lowered entities for the current batch.

This allows REPL/script execution to load only new code into the VM instead of rebuilding the
entire bytecode program on each statement.

`ReplLoweringContext` is intended to be owned by a single REPL session and used from a single
threaded compilation flow; sharing the context across concurrent sessions is not supported.
