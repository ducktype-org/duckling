# HELIOS REPL Utilities

This folder contains HELIOS-level helpers for REPL/script wrapper generation.

For REPL session overview, see: [REPL Module](../../../../../repl/readme.md)

The primary components in this flow are:

1. **[`QueryReplExpressionWrapper`](./repl_queries.hpp):** Builds synthetic HOUT wrapper functions for expression statements.
2. **[`QueryReplInstructionWrapper`](./repl_queries.hpp):** Builds synthetic HOUT wrapper functions for instruction statements (`if`, `while`, `for`, `block`, assignment-routed statements).
3. **[`queryScriptMainRootScope`](./script_helpers.hpp):** Resolves canonical root scope for generated script `main`.
4. **[`buildScriptMainWrapper`](./script_helpers.hpp):** Builds synthetic global `main` that calls generated statement wrappers in order and returns `i64` zero.

## Execution Notes

- Expression wrappers emit `ReturnStmt` for value expressions; void-typed expression path emits `ExprStmt`.
- Instruction wrappers compile statement body with unit return type and append explicit `VoidReturnStmt`.
- Script main wrapper sequences wrapper calls for side effects and satisfies executable entrypoint conventions.
- Wrapper functions are lowered incrementally via DVM `ReplDVMCodeBuilder`, which snapshots the
	program state and emits only newly-lowered code for each REPL/script statement batch.

## Related Documentation

- [REPL Module](../../../../../repl/readme.md)
- [Driver REPL Utilities](../../../../../driver/driver/src/driver/repl_utils/readme.md)
- [HELIOS Module](../../../readme.md)
