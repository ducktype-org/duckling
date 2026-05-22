# PST parser

Building parse/semantic trees (PST) from files and macro expansion.

\parallel getFilePST and QueryMacroExpansion create PST instances; builders/maps used must be safe.

\subpage element-hierarchy

## Statement Extraction Helpers

REPL and script execution use single-statement extraction helpers from:

* [utility.hpp](./src/frontend/pst_parser/utility.hpp)
* [utility.cpp](./src/frontend/pst_parser/utility.cpp)

These helpers provide a strict "exactly one top-level statement" API:

1. `extractSingleStatement`
	Returns a statement only if the root has exactly one child and that child is a statement.

2. `extractSingleExpression`
	Returns a statement only if the single statement is `ExprStmt`.

3. `extractSingleInstruction`
	Returns a statement only if the single statement is one of:
	- `If`
	- `While`
	- `For`
	- `Block`

4. `extractSingleDefinition`
	Returns a statement only if `Stmt::isDeclaration() != DeclKind::None`.

## How REPL/Script Uses Them

The high-level classification in REPL/script is implemented in driver helpers and built on top of these PST utilities:

* [Driver REPL Utilities](../../../driver/driver/src/driver/repl_utils/readme.md)

The flow is:

1. Input is split into top-level statement sources.
2. Each statement is parsed as a one-statement module.
3. Extraction helpers determine whether it is expression, instruction, or definition.

Important detail:

* Assignments are syntactically expressions (`ExprStmt`), but REPL/script routes them through instruction handling in driver classification logic because their lowering currently lives in the instruction pipeline.

## Related Documentation

* [REPL Module](../../../repl/readme.md)
* [Driver REPL Utilities](../../../driver/driver/src/driver/repl_utils/readme.md)