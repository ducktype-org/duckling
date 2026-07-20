# Duckling Virtual Machine

The DVM is organized into a hierarchical, layered architecture. All interactions flow from a high-level user
interface down to the core execution engine.

The primary components in this flow are:

1.  **User (API / CLI):** The entry point for all interactions. A user submits requests to run programs,
    send input, or retrieve output through either a API or a CLI.

2.  **[`Supervisor`](./src/vm/core/supervisor/readme.md):** The top-level manager and request router. The Supervisor
is a singleton that manages the lifecycle of all processes running within the DVM. Its primary job is to receive
requests from the user and delegate them to the correct `VMProcess` based on a unique Process ID (`PID`).

3.  **[`VMProcess`](./src/vm/core/process/readme.md):** An isolated sandbox for a single program. A `VMProcess`
encapsulates everything a single program needs to run, including its code, memory, and execution threads. It is
the central orchestrator that owns and manages the more specialized modules below it.

4.  **[`VMThread`](./src/vm/core/thread/readme.md):** The core execution engine. Responsible for running the
interpreter loop that fetches and executes bytecode instructions one by one. A `VMProcess` can manage one or
more `VMThread`s.

5.  **[`Loader`](./src/vm/loader/readme.md):** The bridge from source code to executable bytecode. Used by the
`VMProcess`, the `Loader` is responsible for parsing, validating, and compiling source code into a low-level, optimized
representation that the `VMThread` can execute directly.

6.  **[`Validator`](./src/vm/bytecode/validator/readme.md):**
A part of the loader module responsible for static analysis that verifies bytecode correctness before execution.
It enforces correctness rules, such as valid instruction arguments and control flow correctness, to ensure that
only safe code is passed to the execution engine.

7.  **[`Memory Module`](./src/vm/core/process/memory/readme.md):** The safe memory governor. Each `VMProcess` has
its own dedicated Memory Module that manages all allocations, enforces memory safety (preventing use-after-free,
buffer overflows, etc.).

8.  **[`VMValue`](./src/vm/core/thread/readme.md#vmvalue):** A data transfer object that acts as the bridge for
moving data into and out of the VM's environment. It is the primary mechanism for passing arguments to functions
and retrieving their return values.

## Detailed Documentation

*   [Benchmark](./benchmark/readme.md)
*   [API](./src/vm/api/readme.md)
*   [Bytecode Validator](./src/vm/bytecode/validator/readme.md)
*   [Compiler](./src/vm/loader/compiler/readme.md)
*   [Supervisor](./src/vm/core/supervisor/readme.md)
*   [VMProcess](./src/vm/core/process/readme.md)
*   [VMThread](./src/vm/core/thread/readme.md)
*   [Memory Module](./src/vm/core/process/memory/readme.md)
*   [Concurrency](./src/vm/core/process/concurrency/readme.md)
*   [VmValue](./src/vm/core/thread/readme.md#vmvalue)
*   [Tests](./tests/readme.md)
