# Driver REPL Utilities

This folder contains driver-level helpers used by REPL and script execution.

For REPL session overview, see: [REPL Module](../../../../../repl/readme.md)

The primary components in this flow are:

1. **[`repl_split_helpers`](./repl_split_helpers.hpp):** Splits raw input into top-level statement source slices in source order.
2. **[`repl_statement_helpers`](./repl_statement_helpers.hpp):** Creates ephemeral chained modules, classifies single statements, and builds executable wrappers for expression/instruction statements.
3. **[`repl_dvm_helpers`](./repl_dvm_helpers.hpp):** Compiles HOUT to DVM code, loads code into a running VM process, and captures expression return values for supported types.
4. **[`script_helpers`](./script_helpers.hpp):** Script-specific helpers for stable synthetic script module IDs and merging per-statement LIR chunks.

## Execution Notes

- Statement splitting uses a probe REPL module configured for script-style top-level parsing order.
- Statement classification routes assignments as instructions (not expressions) because assignment lowering currently lives in the instruction compilation path.
- Definition statements do not use executable wrappers; they are loaded from module HOUT.
- DVM loading uses persistent `ReplLoweringContext` owned by caller; helpers require active context consistency.
- The lowering path snapshots the context and loads only newly-lowered types/functions/globals
	via `captureLoweredEntitiesSnapshot()` + `collectNewCodeSince()`.
- Callers must pair `setContext()` and `invalidateContext()` around `compileAndLoad()` so the
  lowering context never keeps stale query references.

## Script Execution

The same helper stack also powers `.ds` script handling.

Script execution reuses the REPL-style statement chain, but treats the full file as a single compilation input:

1. Driver initialization stores the active script in `ScriptContext`.
2. If a package root is provided, the first synthetic script statement is anchored into that package tree so the script can resolve modules from the same package.
3. The script source is split into top-level statements using the same statement splitter as REPL input.
4. Each statement is compiled in source order into an ephemeral chained module.
5. Definitions are lowered directly from module HOUT.
6. Executable statements are wrapped, lowered, and sequenced under a synthetic script `main`.
7. When running on DVM, imported modules are compiled and loaded before the script bytecode, so dependencies are available when the script starts.

This is why scripts cooperate well with REPL-style code: both flows share the same statement extraction, module chaining, and wrapper generation, and both rely on the same module lookup model.

## Related Documentation

- [REPL Module](../../../../../repl/readme.md)
- [PST Parser Statement Extraction Helpers](../../../../../core/frontend/pst_parser/readme.md#statement-extraction-helpers)
- [HELIOS REPL Utilities](../../../../../core/helios/src/helios/repl_utils/readme.md)
- [Driver Module](../../../readme.md)
