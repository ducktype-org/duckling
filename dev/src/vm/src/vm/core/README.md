# DVM - Core architecture overview

## [`Supervisor`](supervisor/supervisor.hpp)
The `Supervisor` is the central, high-level management component of the virtual machine's architecture. 
It acts as the primary dispatcher, mediating all communication between the outside world (e.g., via an API) 
and the individual processes (`VMProcess`) running within the machine.

The `Supervisor` is the sole module responsible for creating and terminating virtual processes. It maintains 
a central registry of all active processes currently running within DVM. All external commands targeting 
a specific process (such as executing code or handling I/O) are first sent to the `Supervisor` and based on 
the Process ID (`PID`) included in the request, the `Supervisor` locates the corresponding `VMProcess` and forwards
the command for execution.

## [`VMProcess`](process/vmprocess.hpp)
The `VMProcess` is the core component that represents a single, isolated execution environment for a program 
running within the virtual machine. While the `Supervisor` manages multiple processes, the `VMProcess` is 
concerned with everything needed to run *one* specific program from start to finish. It does not execute 
bytecode directly, instead it orchestrates all the necessary resources and delegates the actual execution 
to one or more `VMThread`s.

All management operations on a `VMProcess` (like loading code or requesting output) are performed in the 
context of the caller's thread (e.g., the Supervisor's), ensuring that the core program logic runs without 
interruption.

### Core Responsibilities

1.  **Program Loading and Management:** `VMProcess` is responsible for managing the entire loading pipeline. 
It receives source code, utilizes an internal [`Loader`](../loader/logger.hpp) module to parse and compile 
it into executable bytecode, and stores the resulting program, making it ready for execution.

2.  **Memory Governance:** Each process has its own dedicated and isolated memory space, managed by an internal
[`Memory`](./process/memory/memory.hpp) module. This ensures that one process cannot interfere with the memory of another.

3.  **Thread and Execution Management:** `VMProcess` manages the lifecycle of `VMThread` instances, which are the
actual execution units that interpret and run the bytecode. Its responsibilities include:
  *   Creating new threads to execute requested functions.
  *   Controlling the execution state of the process (e.g., running, paused, stopped).
  *   Passing requests and commands down to the appropriate `VMThread`.

4.  **I/O Handling:** The process manages its own input and output streams (`ProcIO`). It provides an API to 
send input to the running program, retrieve its output, and can "attach" its I/O to external streams (like 
the system's standard input/output) for interactive sessions.

5.  **VmValue Lifetime Management:** `VMProcess` acts as a factory and owner for [`VmValue`](./thread/vmvalue.hpp) 
objects which are used to pass values to DVM from the outside world. More on [`VMValue`](./thread/vmvalue.hpp) 
can be found in TODOP

## [`VMThread`](./thread/vmthread.hpp)
The `VMThread` is the primary execution engine of DVM. While a `VMProcess` manages the overall environment 
for a program, the `VMThread` is the component that actually interprets and executes the 
[low-level bytecode](./thread/low_program/low_program.hpp) instructions, one by one. Each `VMThread` represents
a single thread of execution within a process.

### Core Responsibilities
#### Bytecode Interpretation and Execution
At its core, the `VMThread` runs a continuous execution loop.
In each cycle, it fetches the next instruction from the program and executes the corresponding operation. 
This loop continues until the program terminates, encounters an error, or is paused by an external command. 
It operates exclusively on the [`LowVMProgram`](./thread/low_program/low_program.hpp) representation. DVM 
provides a few execution strategies:
  *   **Switch-Case (SC) Mode:** In this mode, each instruction is identified by a numerical opcode
  (an index in the `OPFUNS` array defined in 
  [`opcodes_functions.hpp`](./thread/opcode_functions/opcodes_functions.hpp)).
  The execution loop uses a large `switch` statement to jump to the correct implementation code. This is 
  straightforward but can be slower due to branch prediction overhead.
  *   **Computed Gotos (CG) Mode:** In this mode, each instruction is identified by a numerical opcode, 
  but the big `switch` statement is replaced with a single `goto` to the function implementing the next opcode. 
  *   **Tail-Call (TC) Mode:** In this mode, each instruction in contains a direct pointer to the function 
  that implements it. This replaces the `switch` statement with an jump to the function which implements 
  the next opcode, which is significantly faster.

The [`MicroInstruction`](./thread/low_program/instruction.hpp) struct is designed to accommodate all of the modes,
using a `union` to store either an opcode index or a function pointer, while keeping the total instruction size fixed
at 24 bytes (8 for the opcode/pointer and 16 for two 64-bit arguments).
```cpp
struct MicroInstruction {
  union { u64 nontc_opcode; OpFunTC* tc_opfun; };
  u64 arg0;
  u64 arg1;
};
```

#### Runtime Environment Management
During execution, the `VMThread` is exclusively responsible for managing the active runtime state for its 
specific thread of control. The key components of this environment are:
*   **Memory Interaction:** The `VMThread` does not own any memory. Instead, it operates on dedicated 
memory regions provided by the `Memory` module owned by its parent `VMProcess`:
    1.  **[Thread Stack](./process/memory/thread_stack.hpp):** This is a stack of 
   [`Frame`](./process/memory/frame.hpp) structures. 
    2.  **[RuntimeData](./process/memory/vmthread.hpp):** 
    3. **Execution Context:** The state of the machine is passed directly as arguments to each opcode's
    implementation function. At any point in time, an instruction has immediate access to its complete
    execution context:
        *   `instr`: A pointer to the current `MicroInstruction` being executed.
        *   `frame`: A pointer to the current function's `Frame` on the call stack.
        *   `local_stack`: A pointer to the base of the current function's variable space within the larger local data stack.
        *   `thread`: A reference to the `VMThread` instance, used to access process-level services like using 
            the using memory blocks, creating `VmValue` objects or interacting with built-in functions.

This design ensures that when an opcode like `call_func` is executed, it can efficiently push a new `Frame`, 
advance the `local_stack` pointer, and jump the `instr` pointer to the first instruction of the new function. 
Similarly, operations like `call_builtin_func` use this context to read arguments from the local stack, bridge 
them to the native C++ environment, execute the native code, and write the result back into the VM's memory.


#### Runtime Environment Management
The `VMThread` acts as the execution engine, but it does not own the resources it operates on. The central 
authority for all memory-related matters is the `Memory` module, which belongs to the parent `VMProcess`. 
The `VMThread` is merely a *user* of this memory.

This process can be broken down into three key concepts:
1.  **Memory Allocation by `Memory`:**
    For each `VMThread`, the `Memory` module allocates a dedicated `ThreadStack` object. This object contains 
    two distinct, pre-allocated memory regions:
    *   **[`ThreadStack`](./process/memory/thread_stack.hpp):** This is a vector of 
    [`Frame`](./process/memory/frame.hpp) structures. Each `Frame` represents a single active function call 
   and stores crucial data needed for execution, such as the local stack pointer, instruction pointer to return 
   to when returning from the function, information about the local variables used by the function, the amount 
   of the local stack used by the function and additional data needed when returning from a function.
    *   **The Local Data Stack:** This is a single, large, contiguous block of raw bytes. All local variables for 
    a function are allocated within this block. `Frame` structures reference this contiguous block of memory when
    assigning the local stack to a called function.

2.  **The Live Execution Context (`FUNCTION_ARGS`):**
    The execution context is **passed as arguments** to every opcode implementation function, as defined by the
    [`FUNCTION_ARGS`](./thread/opcode_functions/opcodes_functions.hpp) macro. This "live" context contains:
    *   `instr`: A pointer to the current `MicroInstruction` being executed.
    *   `frame`: A pointer to the current `Frame` on the call stack.
    *   `local_stack`: A pointer to the base of the *current function's* variable space within the large local data stack block.
    *   `thread`: A reference to the `VMThread` instance, used to access process-level services like using 
        the using memory blocks, creating `VmValue` objects or interacting with built-in functions.

When an opcode like `call_func` is executed, it can efficiently push a new `Frame`, advance the `local_stack` pointer, 
and jump the `instr` pointer to the first instruction of the new function. 

#### Execution State and Flow Control
The `VMThread` is responsible for managing its own execution state, which is crucial for features like debugging.
It operates based on an "execution strategy" that can be one of several modes:
  *   **Normal:** Executes instructions continuously.
  *   **StepByStep:** Executes a single instruction and then immediately transitions to the `Paused` state.
  *   **Paused:** Halts execution and waits for an external command (like `Step` or `Resume`) before proceeding.
  *   **Stopped:** Completely terminates the execution loop.
  
Before each instruction it's checked whether the execution strategy has changed.

#### Handling Blocking Operations
When a program needs to perform a blocking operation, such as waiting for user input, it is the `VMThread` that pauses its execution loop and waits for the necessary data to become available before resuming.


## VmValue
The [`VmValue`](./thread/vmvalue.hpp) class is the key mechanism for bidirectional communication between the 
external world (e.g the compiler or other C++ code) and the virtual machine's internal environment. It 
functions as a data Transfer Object designed to safely package, transfer, and unpack data across in DVM.

Its primary purpose is to enable the transfer of values that may have complex, hierarchical structures, 
not just simple byte streams.

### Key Aspects

1.  **Integration with the Memory module:** A `VmValue` is not just a simple byte buffer. Internally, it 
    holds its data in a `std::vector<byte>`, but critically, it also uses a special mechanism to register 
    the data with the parent process's `Memory` module. As a result, from the VM's perspective, the data 
    within a `VmValue` looks and behaves like a native, fully-functional memory `Block`. This allows standard 
    memory operations, such as creating nested child blocks, copying complex data structures and reference 
    counting, to be performed on it.

2.  **The Bridge for Input and Output:** `VmValue` is the standard method for interacting with executing code. 
    Its primary use cases include:
    *   **Passing Arguments to Functions:** When a user wants to call a function inside the VM, its arguments 
        are first packaged into `VmValue` objects. The execution engine (`VMThread`) then treats these as 
        source memory blocks from which to read the input data. Typically the argument values are created 
        with the `vm::api::getVmValue()` endpoint, filled in with the appropriate data and passed to the `vm::api::runFunction()` endpoint. An important note is that all the `VmValue` object created by the `getVmValue()` endpoint are owned by the caller. This means they are expected to be freed by the caller. If not freed they will be counted as memory leaks when calling `vm::api::deinitAndValidate()`.
    *   **Receiving Results:** After execution completes, the function's return value (or the entire program's 
        exit code) is packaged into a `VmValue` which can be read with the `vm::api::getExitCode()` endpoint 
        and interpreted. Note that `VmValues` returned by the `vm::api::getExitCode()` are owned by the
        `VMProcess` and are automatically freed when the process in destructed.
    Some may ask, why not all `VMValue` objects are owned by the process. This is done from the performance
    standpoint. When calling functions in the VM multiple times we want to avoid the memory bloat which we 
    may encounter by creating many function arguments.