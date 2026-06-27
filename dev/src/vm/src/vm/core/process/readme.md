# DVM — VMProcess module
## [`VMProcess`](./ivmprocess.hpp)
The `VMProcess` is the core component that represents a single, isolated execution environment for a program
running within the virtual machine. While the `Supervisor` manages multiple processes, the `VMProcess` is
concerned with everything needed to run *one* specific program from start to finish. It does not execute
bytecode directly, instead it orchestrates all the necessary resources and delegates the actual execution
to one or more `VMThread`s.

All management operations on a `VMProcess` (like loading code or requesting output) are performed in the
caller's thread (e.g., the Supervisor's thread or a CLI's main thread). This ensures that the `VMThread`
(which is ran on a separate thread) dedicated to running the program's bytecode is not blocked or
interrupted by these administrative tasks.

### Core Responsibilities

1.  **Program Loading and Management:** `VMProcess` is responsible for managing the entire loading pipeline.
It receives source code, utilizes an internal [`Loader`](../../loader/logger.hpp) module to parse and compile
it into executable bytecode, and stores the resulting program, making it ready for execution.

2.  **Memory Governance:** Each process has its own dedicated and isolated memory space, managed by an internal
[`Memory`](./memory/memory.hpp) module. This ensures that one process cannot interfere with the memory of another.

3.  **Thread and Execution Management:** `VMProcess` manages the lifecycle of `VMThread` instances, which are the
actual execution units that interpret and run the bytecode. Its responsibilities include:
  *   Creating new threads to execute requested functions.
  *   Managing the Global Interpreter Lock (GIL) which ensures safe concurrent bytecode execution.
  *   Managing synchronization primitives (mutexes) that DVM programs can use.
  *   Controlling the execution state of the process (e.g., running, paused, stopped).
  *   Passing requests and commands down to the appropriate `VMThread`.

4.  **I/O Handling:** The process manages its own input and output streams (`ProcIO`). It provides an API to
send input to the running program, retrieve its output, and can "attach" its I/O to external streams (like
the system's standard input/output) for interactive sessions.

5.  **VmValue Lifetime Management:** `VMProcess` acts as a factory and owner for [`VmValue`](../thread/vmvalue.hpp)
objects which are used to pass values to DVM from the outside world. More on [`VMValue`](../thread/vmvalue.hpp)
can be found in [here](../thread/readme.md#vmvalue).
