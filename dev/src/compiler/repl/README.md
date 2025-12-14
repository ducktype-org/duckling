# REPL Module

Interactive Read-Eval-Print Loop for the Duckling compiler.

## Structure

- `src/repl/repl_session.hpp` - REPL session class interface
- `src/repl/repl_session.cpp` - REPL session implementation
- `CMakeLists.txt` - Build configuration

## Usage

### Basic Usage

```cpp
#include <repl/repl_session.hpp>

using namespace compiler::repl;

// Create and run a REPL session with default configuration
ReplSession session;
session.run();
```

### REPL Commands

The REPL supports several built-in commands:

- `/help` or `/?` - Show help message with available commands
- `/exit`, `/quit`, or `/q` - Exit the REPL session
- `/history` or `/h` - Display all executed statements with numbering
- `/clear` or `/c` - Clear the statement history

### Multiline Mode

- Enter `"""` (triple quotes) to start multiline input
- Type `/end` on a new line to finish and execute the multiline code
- Much clearer than counting empty lines!

### Custom Configuration

```cpp
ReplConfig config;
config.prompt = "duck> ";
config.continuation = "  ... ";
config.multiline_start = "```";
config.show_hout_debug = false;
config.run_dvm = true;

ReplSession session(config);
session.run();
```

### Programmatic Statement Execution

```cpp
ReplSession session;

// Execute a single statement
ReplResult result = session.executeInput("let x = 42;");

if (result.status == ReplResult::Status::Success) {
    std::cout << "Execution successful!\n";
}

// Access history
for (const auto& stmt : session.getHistory()) {
    std::cout << "Statement: " << stmt.source_code << "\n";
    std::cout << "Module ID: " << stmt.module_id.queryUnstablePerfectHash() << "\n";
}
```

## Key Types

### ReplStatement

Represents a single executed statement with its associated module:

```cpp
struct ReplStatement {
    std::string                     source_code;  // The source code
    base::Ref<frontend::ModuleTree> module;       // Module created from code
    frontend::ModuleID              module_id;    // Module identifier
};
```

### ReplConfig

Configuration for REPL behavior:

```cpp
struct ReplConfig {
    std::string prompt;           // Primary prompt (default: "duckling> ")
    std::string continuation;     // Continuation prompt (default: "      |")
    std::string multiline_start;  // Multiline trigger (default: "\"\"\"")
    bool        show_hout_debug;  // Show HOUT output (default: true)
    bool        run_dvm;          // Execute on DVM (default: true)
};
```

### ReplResult

Result of processing input:

```cpp
struct ReplResult {
    enum class Status { Success, Error, Exit, IncompleteInput };
    
    Status      status;      // Result status
    std::string message;     // Error/status message
    i32         exit_code;   // DVM exit code (if applicable)
};
```

## Integration with Main Compiler

The REPL is integrated into `duckc` as a subcommand:

```cpp
// In main.cpp
.addSubcommand(
    clah::Clah("repl", "Start an interactive REPL session")
        .setHandler([](const clah::ParsingResult& options) -> int {
            compiler::driver::initializeTheCompiler(/* ... */);
            compiler::repl::ReplSession session;
            return session.run();
        })
)
```

Run with:
```bash
./duckc repl
```

## Implementation Notes

- Each statement creates a virtual in-memory module
- Statements are compiled through the full HELIOS pipeline
- History maintains references to all statement modules
- DVM execution is optional and controlled by configuration
