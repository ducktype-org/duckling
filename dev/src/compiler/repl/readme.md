# REPL Module

Interactive Read-Eval-Print Loop (REPL) for the Duckling programming language. Provides an interactive environment for executing Duckling code, evaluating expressions, and exploring language features in real-time.

## Overview

The REPL module enables interactive development by compiling and executing Duckling code incrementally. Each input is processed through the full HELIOS compiler pipeline and executed on the Duckling Virtual Machine (DVM), with support for both single-line expressions and multi-line definitions.

## Features

- **Interactive Execution**: Evaluate expressions and see results immediately
- **Incremental Compilation**: Compile and load definitions into a persistent environment
- **Statement History**: Track all executed statements with full module retention
- **Multiline Input**: Support for complex code blocks with dedicated multiline mode
- **Built-in Commands**: Convenient commands for history, help, and session management
- **Full Pipeline Integration**: Uses the complete HELIOS → LIR → DVM compilation pipeline

## Structure

```
src/repl/
├── session.hpp            # Main REPL session interface
├── session.cpp            # Session implementation and input handling
├── helper_structs.hpp     # Core data structures (ReplConfig, ReplResult, ReplStatement)
├── frontend.hpp           # Terminal I/O and line editing
├── frontend.cpp           # Frontend implementation with history navigation
```

Also some helpers are in other modules:

```
compiler/core/helios/repl_utils/
├── repl_queries.hpp       # HOUT expression wrapper generation
├── repl_queries.cpp       # HOUT expression wrapper generation
```

```
compiler/driver/repl_utils/
├── repl_dvm_helpers.hpp       # DVM compilation and execution utilities
├── repl_dvm_helpers.cpp       # LIR lowering and bytecode generation
```


## Usage

### Running the REPL

From the command line:
```bash
./duckc repl
```

This launches an interactive session where you can enter Duckling code:
```
duckling> let x = 42;
duckling> builtin_output_i64(x + 8);
=> 50
duckling> /exit
```

### Basic API Usage

```cpp
#include <repl/session.hpp>

using namespace compiler::repl;

// Create and run a REPL session with default configuration
ReplSession session;
session.run();
```

### REPL Commands

The REPL provides several built-in commands (all start with `/`):

| Command | Aliases | Description |
|---------|---------|-------------|
| `/help` | `/?` | Display help message with available commands |
| `/exit` | `/quit`, `/q` | Exit the REPL session |
| `/history` | `/h` | Show all executed statements with line numbers |
| `/clear` | `/c` | Clear the statement history |

### Multiline Mode

Just type Alt + Enter to create a new line.

### Custom Configuration

```cpp
ReplConfig config;
config.prompt = "duck> ";              // Primary prompt
```

### Programmatic Execution

```cpp
ReplSession session;

// Execute code programmatically
ReplResult result = session.executeInput("let x = 42;");

if (result.status == ReplResult::Status::Success) {
    std::cout << "Execution successful!\n";
}

// Access execution history
for (const auto& stmt : session.getHistory()) {
    std::cout << "Statement: " << stmt.source_code << "\n";
}
```

## Core Types

### ReplSession

Main class managing the interactive session. Maintains execution history, DVM process, and handles all user input.

**Key Methods:**
- `int run()` - Start the interactive REPL loop (blocking)
- `ReplResult processLine(const std::string& line)` - Process a single line of input
- `ReplResult executeInput(const std::string& input)` - Execute code as expression or definition
- `const std::vector<ReplStatement>& getHistory()` - Access execution history
- `void clearHistory()` - Reset history and line counter

### ReplStatement

Represents a single executed statement with its associated module:

```cpp
struct ReplStatement {
    std::string                     source_code;  // Original source code
    base::Ref<frontend::ModuleTree> module;       // Associated module tree
    frontend::ModuleID              module_id;    // Unique module identifier
};
```

### ReplConfig

Configuration for REPL behavior:

```cpp
struct ReplConfig final {
    static constexpr std::string PROMPT
        = "duckling> ";  /// Primary prompt shown before each input
    static constexpr std::string CONTINUATION = "          ";  /// Prompt for continuation lines
    static constexpr std::string HISTORY_MULTILINE_CONTINUATION
        = "    ";  /// Prompt for history continuation.
};
```

### ReplResult

Result of processing input:

```cpp
struct ReplResult {
    enum class Status { 
        Success,         // Execution completed successfully
        Error,           // Compilation or execution error
        Exit,            // User requested exit
        IncompleteInput  // Input needs continuation (multiline)
    };
    
    Status      status;      // Result status
    std::string message;     // Error/status message
    i32         exit_code;   // DVM exit code (for expressions)
};
```

## Implementation Details

### Compilation Pipeline

1. **Input Processing**: User input is parsed to determine if it's a command or code
2. **Module Creation**: Code is wrapped in a virtual in-memory module
3. **PST Parsing**: Source is parsed into a Parse Syntax Tree (PST)
4. **Expression Detection**: System determines if input is a single expression or definition
5. **HOUT Generation**:
   - **Expressions**: Wrapped in a synthetic function returning the expression value
   - **Definitions**: Compiled as top-level declarations
6. **LIR Lowering**: HOUT is lowered to Low Intermediate Representation (LIR)
7. **DVM Compilation**: LIR is compiled to DVM bytecode
8. **Execution**: Bytecode is loaded and executed on the DVM
9. **Result Handling**: Expression results are extracted and displayed

### Expression Evaluation

Single expressions are automatically wrapped in a function that returns their value:

```duckling
# User input
42 + 8

# Internally becomes
fun __repl_expr_wrapper__0() -> i64 = {
    return 42 + 8;
}
```

The function is executed on the DVM, and the return value is extracted and displayed.

### Supported Types

Expression results are currently supported for the following types:
- `i32` - 32-bit signed integer
- `i64` - 64-bit signed integer
- `f32` - 32-bit floating point
- `f64` - 64-bit floating point
- `bool` - Boolean values
- `void` - No return value (void expressions execute without output)

### History Management

The REPL maintains a complete history of executed statements:
- Each statement retains its associated module in memory
- Statements are numbered sequentially starting from 1
- History can be viewed with `/history` or cleared with `/clear`
- Previously defined symbols remain accessible in subsequent statements

## Integration

The REPL is integrated into the `duckc` compiler as a subcommand. The integration initializes the compiler environment and launches the REPL session:

```cpp
// In duckc main.cpp
.addSubcommand(
    clah::Clah("repl", "Start an interactive REPL session")
        .setHandler([](const clah::ParsingResult& options) -> int {
            compiler::driver::initializeTheCompiler(/* ... */).status();
            compiler::repl::ReplSession session;
            return session.run();
        })
)
```

## Limitations and Future Work

Current limitations:
- Expression result display is limited to primitive types
- No support for displaying complex types (structs, arrays, etc.)
- History is not persisted between sessions
- Limited line editing capabilities (no multi-line history navigation)
- Lookup and tab completion not implemented

Potential future enhancements:
- Persistent history across sessions
- Enhanced line editing with GNU Readline or similar
- Display support for complex data structures
- Tab completion for symbols and keywords
- Syntax highlighting in the terminal
