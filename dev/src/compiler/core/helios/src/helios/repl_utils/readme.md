# HELIOS REPL Utilities

This folder contains HELIOS-level helpers for REPL/script wrapper generation.

For REPL session overview, see: [REPL Module](../../../../../repl/readme.md)

The primary components in this flow are:

1. **[`queryRepl*WrapperSymbol`](./repl_queries.hpp):** Returns the generated symbol of the wrapper of a single input statement: an expression, an instruction (`if`, `while`, `for`, `block`, assignment-routed statements) or the initializer of a global variable.
2. **[`getReplInputFunction`](./repl_queries.hpp):** Builds the HOUT body of such a wrapper. It is the `QueryCodeOfFun` implementation for those symbols, so the wrappers take part in the regular function pipeline, including the transitive collection of generated symbols.
3. **[`queryScriptMainRootScope`](./script_helpers.hpp):** Resolves canonical root scope for generated script `main`.
4. **[`buildScriptMainWrapper`](./script_helpers.hpp):** Builds synthetic global `main` that calls generated statement wrappers in order and returns `i64` zero.

## Execution Notes

- Expression wrappers emit `ReturnStmt` for value expressions; void-typed expression path emits `ExprStmt`.
- Instruction wrappers compile statement body with unit return type and append explicit `VoidReturnStmt`.
- Global variable initializer wrappers emit the construction of the declared initial value, so that it runs in statement order. The storage itself is a separate [`queryReplEmptyVariableSymbol`](./repl_queries.hpp) symbol holding an empty (zero) value, mangled to the name of the declared variable.
- The wrapped statement is identified by its stable PST hash, recovered with `pst::LangElement::getByStableHash`.
- Script main wrapper sequences wrapper calls for side effects and satisfies executable entrypoint conventions.
- Wrapper functions are lowered incrementally via DVM `ReplDVMCodeBuilder`, which snapshots the
	program state and emits only newly-lowered code for each REPL/script statement batch.

## Related Documentation

- [REPL Module](../../../../../repl/readme.md)
- [Driver REPL Utilities](../../../../../driver/driver/src/driver/repl_utils/readme.md)
- [HELIOS Module](../../../readme.md)
