# REPL Module

Interactive Read-Eval-Print Loop (REPL) for the Duckling programming language.

## Overview

The REPL compiles and executes Duckling input incrementally. It runs on a dedicated DVM process and keeps session state between statements.

The primary components in this flow are:

1. **User Input (CLI/API):** Input is entered interactively (`duckc repl`) or preloaded from script (`duckc repl script.ds`, `/load <file.ds>`).
2. **[`ReplSession`](./src/repl/session.hpp):** Main orchestrator. Handles commands, splits code into statements, creates statement modules, and routes execution.
3. **[`ReplFrontend`](./src/repl/frontend.hpp):** Terminal interaction layer (replxx or minimal implementation), including history display and help.
4. **Driver REPL Helpers (`driver/repl_utils`):** Statement splitting/classification, HOUT-to-DVM compilation, and code loading.
5. **HELIOS REPL Helpers (`helios/repl_utils`):** Wrapper-function generation for executable statements.
6. **DVM Process:** Receives newly compiled code and executes wrapper functions.

Execution model notes:
- Input can contain multiple top-level statements.
- Each statement is executed in order.
- Execution is non-transactional: if statement N fails, earlier statements remain applied.
- Symbol lookup across REPL history uses a parent-module chain; root-scope lookup falls back
  to the parent REPL module via HELIOS `QueryLookupInScopeAndParents`.

## Features

- Interactive expression/instruction execution
- Incremental definition loading into session state
- Statement history with retained per-statement module context
- Multi-statement input support
- Script preload and `/load` command
- Full HELIOS -> LIR -> DVM path for execution

## Structure

```
src/repl/
├── session.hpp            # REPL session interface
├── session.cpp            # Session orchestration and execution flow
├── helper_structs.hpp     # ReplConfig, ReplResult, ReplStatement
├── frontend.hpp           # Frontend abstraction
├── frontend.cpp           # Frontend delegation
└── frontend_implementations/
   ├── replxx.*            # Rich frontend (completions/highlighting/history)
   └── minimal.*           # Minimal terminal frontend
```

## Usage

### Running the REPL

```bash
./duckc repl
```

Optional script preload:

```bash
./duckc repl script.ds
```

Disable completions/hints:

```bash
./duckc repl --no-completions
```

### Commands

| Command | Aliases | Description |
|---------|---------|-------------|
| `/help` | `/?`, `/h` | Show help |
| `/exit` | `/quit`, `/q` | Exit session |
| `/history` | `/hist` | Print entered statements |
| `/clear` | `/c` | Clear terminal screen |
| `/load <file.ds>` | - | Load script into current session |

### Editing

- REPLXX frontend:
  - `Enter` submits input
  - `Alt + Enter` inserts newline
- Minimal frontend:
  - `Alt + Enter` inserts newline

### Programmatic Use

```cpp
#include <repl/session.hpp>

using namespace compiler::repl;

ReplSession session;

auto load_result = session.loadScriptFile("./bootstrap.ds");
if (load_result.status == ReplResult::Status::Error) {
    std::cerr << load_result.message << "\n";
    return 1;
}

return session.run();
```

## Core Types

### ReplSession

Main public API:
- `int run()`
- `ReplResult loadScriptFile(std::string_view file_path)`

### ReplStatement

Stores one executed statement and its associated module context.

```cpp
struct ReplStatement {
    std::string                      source_code;
    base::CRef<frontend::ModuleTree> module;
    frontend::ModuleID               module_id;
};
```

### ReplConfig

Prompt-related constants used by frontend implementations.

### ReplResult

```cpp
struct ReplResult {
    enum class Status { Success, Error, Exit, IncompleteInput };
    Status      status;
    std::string message;
};
```

## Execution Flow

For each statement:

1. Input is split into top-level statements.
2. A statement module is created (with REPL parent link to previous statement module when available).
3. The statement is classified as expression, instruction, or definition.
4. Expression/instruction paths generate wrapper functions; definition path loads module HOUT.
5. HOUT is lowered to LIR and compiled to DVM bytecode.
6. Lowering uses `ReplLoweringContext` to emit only newly-lowered entities
  (see `core/backends/dvm/README.md`).
7. New code is loaded into current DVM process.
8. Wrapper executes (for executable statements) and result is printed when supported.

### Statement Kinds

- **Expression:** evaluated through generated wrapper returning a value.
- **Instruction:** executed for side effects via generated unit-returning wrapper.
- **Definition:** loaded into session state without wrapper execution.

Assignments are routed as instructions.

### Supported Printed Result Types

- `i32`
- `i64`
- `f32`
- `f64`
- `bool`
- `()` (unit; no printed value)

## Related Documentation

- [Compiler Overview](../readme.md)
- [Driver REPL Utilities](../driver/driver/src/driver/repl_utils/readme.md)
- [Script Execution](../driver/driver/src/driver/repl_utils/readme.md#script-execution)
- [HELIOS REPL Utilities](../core/helios/src/helios/repl_utils/readme.md)
- [PST Parser Statement Extraction Helpers](../core/frontend/pst_parser/readme.md#statement-extraction-helpers)
- [Driver Module](../driver/driver/readme.md)
- [VM Overview](../../vm/readme.md)

## Limitations

- Printed expression results currently support only selected primitive types and unit.
- Complex structured values are not pretty-printed yet.
- Expression result extraction still uses per-type conversion logic in driver REPL helpers.
- REPL does not allow for overwriting already defined symbols or functions.
  
