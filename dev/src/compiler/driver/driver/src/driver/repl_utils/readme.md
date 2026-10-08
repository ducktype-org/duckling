# Driver REPL Utilities

This folder contains driver-level helpers used by REPL and script execution.

For REPL session overview, see: [REPL Module](../../../../../repl/readme.md)

The primary components in this flow are:

1. **[`repl_split_helpers`](./repl_split_helpers.hpp):** Splits raw input into top-level statement source slices in source order.
2. **[`repl_statement_helpers`](./repl_statement_helpers.hpp):** Creates synthetic chained modules, classifies single statements, and builds executable wrappers for expression/instruction statements and for global variable declarations.
3. **[`repl_dvm_helpers`](./repl_dvm_helpers.hpp):** Compiles HOUT to DVM code, loads code into a running VM process, and captures expression return values for supported types.
4. **[`script_helpers`](./script_helpers.hpp):** Script-specific helpers for stable synthetic script module IDs and merging per-statement LIR chunks.

## Execution Notes

- Statement splitting uses a probe REPL module configured for script-style top-level parsing order.
- Statement classification routes assignments as instructions (not expressions) because assignment lowering currently lives in the instruction compilation path.
- Definition statements do not use executable wrappers; they are loaded from module HOUT.
- A variable declaration is classified on its own: `buildVariableWrapper` splits it into the storage of the variable, holding an empty value, and a wrapper function constructing its declared initial value.
- DVM loading uses persistent `ReplDVMCodeBuilder` owned by caller; helpers require active context consistency.
- The lowering path inserts a new `lir::LIRUnit` into the builder and loads only newly-lowered code
	via `insertLIRUnitAndCollectNewlyLoweredCode(lir_unit)`.
- Callers must pair `setContext()` and `invalidateContext()` around `compileAndLoad()` so the
  lowering context never keeps stale query references.

## Script Execution

The same helper stack also powers `.dks` script handling.

Script execution reuses the REPL-style statement chain, but treats the full file as a single compilation input:

1. Driver initialization stores the active script in `ScriptContext`.
2. The script source is split into top-level statements using the same statement splitter as REPL input.
3. Each statement is compiled in source order into an synthetic chained module.
4. Definitions are lowered directly from module HOUT.
5. Executable statements are wrapped, lowered, and sequenced under a synthetic script `main`.
6. Variable declarations contribute their storage and an initializer wrapper, which `main` calls at the source position of the declaration, instead of before it runs together with every other global.
7. When running on DVM, the standard library is merged into the script bytecode, so dependencies are available when the script starts.

This is why scripts cooperate well with REPL-style code: both flows share the same statement extraction, module chaining, and wrapper generation, and both rely on the same module lookup model.

## Related Documentation

- [REPL Module](../../../../../repl/readme.md)
- [PST Parser Statement Extraction Helpers](../../../../../core/frontend/pst_parser/readme.md#statement-extraction-helpers)
- [HELIOS REPL Utilities](../../../../../core/helios/src/helios/repl_utils/readme.md)
- [Driver Module](../../../readme.md)
