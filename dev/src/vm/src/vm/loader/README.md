# Part III
# Code Loading

---

# Chapter 6
# Architecture of the Bytecode Loading Flow into the Virtual Machine

This chapter presents a detailed architecture of the process of loading, verifying, and compiling bytecode in the system we designed. The goal was to create a flexible yet secure processing pipeline that allows for both dynamic attachment of new code to a running machine and control over its correctness at every stage. The described architecture forms the foundation for the stability and usability of the entire virtual machine.

## 6.1. Design Assumptions

When designing the architecture, we aimed to meet the assumptions we set, which determined the structure and behavior of the individual system components:

-   **Enabling dynamic code injection:** The system must allow for the addition of new code fragments (functions, types, global data) to an already running virtual machine process without the need for its restart. Every such operation must be transactional—in case of an error, the system reverts to the previous, correct state.

-   **Support for multi-file programs:** The architecture must support programs composed of many separate source files and ensure the consistency of the entire program.

-   **Modularity and separation of responsibilities:** The loading process has been divided into logical, independent modules (e.g., parser, type validator, function validator, compiler), each responsible for a specific processing stage. Such a division facilitates development, testing, and potential system expansion.

-   **Static verification:** Every code fragment, before being allowed for execution, goes through validation, which checks for structural and logical correctness, as well as type compatibility.

-   **Room for future optimizations:** The loading process concludes with compilation to a low-level representation, called "micro-bytecode." This stage was intentionally separated to allow for the future introduction of more advanced optimization techniques described in section 19.3.1.

---

## 6.2. Code Representations in the Processing Pipeline

As the code flows through the system's modules, it is transformed and stored in various forms. Each subsequent representation is enriched with additional information and guarantees, bringing the code closer to an executable form.

### Source File (`fs::FilePath`)

The original and most basic form is a text file containing bytecode in a textual format. In the future, this format may be extended with a binary variant, which will eliminate the need for parsing files and reduce their size on disk.

### Token Stream (`tokenizer::TokenSource`)

In the first stage of processing, the source file is transformed by the tokenizer into a sequence of tokens. The `TokenSource` structure (Figure 6.1) stores not only the tokens themselves but also information about their location in the source file (line and column number), which is later used to generate clear and accurate error messages.

```cpp
struct TokenSource {
    // Information about the location of tokens in the source file.
    dia::Location location;
    // Stream of tokens.
    std::vector<lexer::Token> token_data;
};
```
*Figure 6.1: The TokenSource structure storing a stream of tokens along with their location in the file.*

### Parsed File (`ParsedFile`)

The parser builds a syntax tree based on the token stream, with the `ParsedFile` structure (Figure 6.2) as its root. Each source file is represented by one such structure. It is a high-level, direct abstraction of the file's content, containing only information that can be read from it without deeper semantic analysis. At this stage, instructions and types are still identified by character strings (`string`), and structures like virtual tables (v-tables) are not yet built.

```cpp
struct ParsedFile {
    // List of functions in the file.
    std::vector<ParsedFunc> functions;
    // List of types in the file.
    std::vector<ParsedType> types;
    // List of global variables in the file
    std::vector<ParsedGlobalData> globals;
    fs::FilePath source_file;
};
```
*Figure 6.2: The ParsedFile structure representing the content of a single file with code.*

### Code Collection (`CodeCollection`)

After parsing all the files that are part of the program, their contents are combined into a single `CodeCollection` structure (Figure 6.3), which represents the source code and types of the entire program. This structure is the main input for the validation module.

```cpp
struct CodeCollection {
    std::vector<Function> functions;
    std::vector<TypeOfData> types;
    std::vector<GlobalData> global_data;
};
```
*Figure 6.3: The CodeCollection structure aggregating code from all program files.*

The key difference compared to `ParsedFile` is the representation of instructions—instead of raw strings, a variant (`std::variant`) is used here, which encloses all possible instructions in the C++ type system.

```cpp
using Instruction = std::variant<
    Op_mov_i8_imm, Op_mov_i8_i8, Op_cmov_i8_i8,
    Op_mov_i16_imm, Op_mov_i16_i16, Op_cmov_i16_i16,
    Op_mov_i32_imm, Op_mov_i32_i32, Op_cmov_i32_i32,
    // (...)
    Op_label, Op_breakpoint, Op_exit
>;

struct Function {
    Identifier name;
    std::vector<Instruction> code;
};
```
*Figure 6.4: The Function structure storing information about a single function.*

The types of arguments passed to instructions are represented in a similar way.

```cpp
using OpCodeArg = std::variant<
    StackLocalI8, StackLocalI16, StackLocalI32, StackLocalI64,
    StackLocalAny, StackLocalPtr, StackLocalVnt,
    GlobalI64, GlobalI32, GlobalI16, GlobalI8, GlobalPtr,
    Immediate, Type, Field, FunctionName, MethodName, Label
>;
```
*Figure 6.5: The OpCodeArg structure representing the type of a single argument.*

### Valid Program (`ValidProgram`)

`ValidProgram` (Figure 6.6) is the main, high-level representation of the program maintained within the `Loader` module. Its invariant is the guarantee that the state stored in it is always correct. Any modification attempt (e.g., by injecting new code) is a transactional operation.

```cpp
class ValidProgram {
public:
    // Injects code into the current state.
    // Throws exceptions in case of verification errors.
    void insertCode(const code::CodeCollection& collection);
private:
    StableObjIdNameMap<code::Function> function_map;
    StableObjIdNameMap<code::GlobalData> globals_map;
    StableObjIdNameMap<code::TypeOfData> types;
};
```
*Figure 6.6: Interface of the ValidProgram class managing the correct state of the program.*

The `insertCode` method first verifies the new code, also taking the old context into account, and only after all checks are passed does it update the internal state. In case of an error, the old state remains untouched, and an exception is reported to the caller with a description of the error that occurred. The initial state of `ValidProgram` is initialized with built-in types, such as `i64`, `ptr_i64`, `void`, or the type of the `main` function, which was described in more detail in Chapter 15.

The `StableObjIdNameMap` structure is a dictionary that allows access to objects using one of two keys—the name or the identifier of the object. Crucially from an implementation standpoint, objects are not reallocated, so pointers to them remain valid despite modifications to the structure. This dictionary plays a key role in the context of efficient code injection—existing pointers and objects can still be used without the need for their recreation.

### Micro-bytecode — Low-level machine program (`LowVMProgram`)

After successful validation, `ValidProgram` is passed to the compiler, which translates it into `LowVMProgram` (Figure 6.7)—a low-level, optimized, and, above all, understandable for the execution module representation, also known as micro-bytecode. At this stage, all symbolic names are replaced by their numeric counterparts required for the fast operation of the execution module:

-   **Instructions** (previously as variants) are translated into indices in an implementation table or, if the machine is run in tail-call mode, into direct pointers to functions implementing the instruction's behavior.

-   **Names of local variables** are replaced by their offsets on the local stack.

-   **Names of functions, methods, types, and global variables** are converted into unique identifiers (numbers, their indices in respective tables) that allow access at runtime.

This form is the final product of the loading pipeline, and it is passed to the virtual machine's execution module. In the remainder of this chapter, we present part of the implementation of the low-level representation of bytecode:

```cpp
struct LowVMProgram {
    // Mapping of a function's index to its corresponding structure.
    StableObjIdNameMap<FuncData> functions;
    // Mapping of a global variable's index to its type.
    StableObjIdNameMap<TypeCRef> global_data;
    // Mapping of numeric method identifiers to their names.
    std::unordered_map<u64, std::string> method_name_pool;
    TypeMetadata types;
};
```
*Figure 6.7: The LowVMProgram structure—a low-level representation of the program.*

Each function in `LowVMProgram` is described by the `FuncData` structure (Figure 6.8), which contains the compiled micro-bytecode and the metadata necessary for its execution.

```cpp
struct FuncData {
    // Function name.
    base::StrID name;
    // List of function instructions.
    std::vector<Fix8Instruction> bytecode;
    // Total size of the local stack used by the function.
    std::size_t local_stack_size;
    // Total size of the function's arguments.
    std::size_t arg_size;
    // Size of the result type.
    std::size_t ret_size;
};
```
*Figure 6.8: The FuncData structure storing compiled code and function metadata.*

The instructions themselves are represented by the `Fix8Instruction` structure (Figure 6.9), which, depending on the execution mode, stores the instruction as an index in the implementation table or a direct pointer to the function implementing the instruction.

```cpp
struct Fix8Instruction {
    union {
        // Index in the implementation table.
        u64 nontc_opcode;
        // Pointer to the function implementing the instruction.
        OpFunTC* tc_opfun;
    };
    i32 arg0; // Numeric representation of the first argument.
    i32 arg1; // Numeric representation of the second argument.
};
```
*Figure 6.9: Low-level representation of a single micro-bytecode instruction.*

### Representation of Types

During loading, the way types are represented in the program also changes. Initially, in the `ParsedFile` structure, types are represented by the `ParsedType` structure, which contains only information read from the file (e.g., names of base types and subtypes as `string`). The next step is conversion to the `TypeOfData` type, which stores types as variants (`std::variant`), similar to instructions and argument types. After passing through the validator (`TypeValidator`), types are transformed and added to the `TypeMetadata` collection, which stores `Type` objects—fully expanded objects, enriched with all the information needed at runtime, such as built virtual method tables (v-tables), fields inherited from superclasses, and direct references to subtypes (other `Type` objects) instead of their names.

## 6.3. High-Level Data Flow Architecture

The main component managing the program's life cycle is `VMProcess`. It stores the current, executable state of the program in the form of `LowVMProgram`. Each `VMProcess` has its own `Loader` module object, which in turn maintains a consistent, high-level representation of the same code in the form of a `ValidProgram` object.

The data flow is initiated by an API request to load code, e.g., `loadFile`.

1.  `VMProcess` receives the request and passes it to its `Loader` object.

2.  The `Loader` processes the file(s), verifies the code through the `TypeValidator` and `FunctionValidator` modules, and tries to inject it into its internal state (`ValidProgram`).

3.  If the operation succeeds, the `Loader` updates its state, compiles a new, complete version of the program into the `LowVMProgram` form, and passes it to `VMProcess`.

4.  `VMProcess` replaces its old version of the executable code with the new one.

5.  If an error occurs at any stage of verification, the `Loader` interrupts the operation, its state remains unchanged, and a detailed description of the error is passed to `VMProcess`, which in turn is passed to the user via an API response. The executable state in `VMProcess` also does not change.

This model guarantees that the virtual machine process always operates on correct and consistent code. When `VMProcess` receives an execution request (`run`/`runFunction`), it starts a thread (`VMThread`) on the currently stored `LowVMProgram` object.

*Figure 6.10: Architecture of the loading module.*

**Translation of labels from the diagram in Figure 6.10:**
- **Boxes (Modules/Components):**
    - `VMProcess`: VMProcess
    - `Użytkownik / Kompilator`: User / Compiler
    - `Loader`: Loader
    - `Parser`: Parser
    - `Tokenizer`: Tokenizer
    - `ValidProgram`: ValidProgram
    - `TypeValidator`: TypeValidator
    - `FunctionValidator`: FunctionValidator
    - `Kompilator`: Compiler
- **Functions/Actions (inside boxes):**
    - `załadujProgram()`: `loadProgram()`
    - `api::loadCode()`: `api::loadCode()`
    - `zapiszStan()`: `saveState()`
    - `stwórzProgram()`: `createProgram()`
    - `for f in files: tokenizuj(), parsuj()`: `for f in files: tokenize(), parse()`
    - `tokenizuj()`: `tokenize()`
    - `wstrzyknijKod()`: `injectCode()`
    - `unieważnijStan()`: `invalidateState()`
    - `for t in types: weryfikuj(t), wstrzyknij(t)`: `for t in types: verify(t), inject(t)`
    - `weryfikuj(t), zbudujTyp(t)`: `verify(t), buildType(t)`
    - `for g in globals: weryfikuj(g), wstrzyknij(g)`: `for g in globals: verify(g), inject(g)`
    - `for f in funcs: weryfikuj(f), wstrzyknij(f)`: `for f in funcs: verify(f), inject(f)`
    - `sprawdzEtykiety()`: `checkLabels()`
    - `for instr in function: sprawdzStrukture(), sprawdzArgumenty(), sprawdzInstrukcje(), sprawdzStanStosu()`: `for instr in function: checkStructure(), checkArguments(), checkInstructions(), checkStackState()`
    - `zaakceptujStan()`: `acceptState()`
    - `kompiluj()`: `compile()`
    - `nazwyNaPrzesunięcie()`: `namesToOffsets()`
    - `wyrzucenieEtykiet()`: `removeLabels()`
    - `wyliczenieSkoków()`: `calculateJumps()`
    - `konwersjaInstrukcji()`: `convertInstructions()`
    - `konwersjaFunkcji()`: `convertFunction()`
- **Data Flow (on arrows):**
    - `{fs::FilePath} / CodeCollection`: `{fs::FilePath} / CodeCollection`
    - `api::LoadOk{} / api::LoadError{}`: `api::LoadOk{} / api::LoadError{}`
    - `fs::FilePath`: `fs::FilePath`
    - `{fs::FilePath}`: `{fs::FilePath}`
    - `CodeCollection`: `CodeCollection`
    - `TokenData`: `TokenData`
    - `TypeOfData`: `TypeOfData`
    - `TypeMetadata`: `TypeMetadata`
    - `Function`: `Function`
    - `ValidProgram`: `ValidProgram`
    - `LowVMProgram`: `LowVMProgram`

---

## How code loading works on the VM

In this subsection, we have described the responsibilities and internal workings of the key modules 
involved in the loading process.

### VMProcess

`VMProcess` is the main module for managing the loading and execution of code, handling requests 
coming from the external API which may come either from a user or a compiler.

std::expected<api::Response, api::LoadProgramError> loadProgram(
    const std::variant<std::vector<fs::FilePath>, std::vector<code::CodeCollection>>& source
);

-   If the request comes from a user, it contains a list of paths to files with a text representation of the bytecode.
-   If the request comes from the compiler, it contains code provided immediately in the form of a `CodeCollection` 
structure, which allows skipping the tokenization and parsing stage.

The main task of `VMProcess` is to delegate tasks related to loading to the `Loader` module and manage the executable state. 
Each `VMProcess` 
- Keeps the current state of the `LowVMProgram` which is the current low level representation (executable on the machine) of the code.
- Keeps it's own instance of the `Loader` module which keep the current high level representation (`ValidProgram`) of the same code 
as the one kept on the `LowVMProgram`
- `VMProcess` is initialized with an empty state of both `LowVMProgram` and `ValidProgram`

- The key concept which acompanies the loading phase is that the state kept in the `VMProcess` and `Loader` is always valid.

Basically, executing `loadProgram` tries to "inject" new code to the current `VMProcess` state and succeeds only if the whole state 
(the old state + the newly injected code) represents a valid program. Is this fails one of the errors (defined in `errors.hpp`) is 
thrown and the state is not updated.



After receiving a new `LowVMProgram` from the `Loader`, `VMProcess` updates its internal executable code state and provides 
the user with information about the successful program loading through an API response. In case of an error, the user is 
provided with information about the loading error, containing a detailed description of the error that occurred. Example 
errors generated by the loading module are described in Appendix A.

### Loader

`Loader` is an improved version of the original implementation, previously called the preprocessor. It is a module that manages high-level code loading and verification.

1.  It receives a list of files or a ready `CodeCollection` structure from `VMProcess` (in case the bytecode comes directly from the compiler).
2.  If it received files, it runs `Tokenizer` and `Parser` to obtain a `CodeCollection` representation. Syntax errors are detected at this stage.
3.  Having `CodeCollection`, it tries to inject the new code into its internal `ValidProgram` object. This operation performs the actual static verification of the injected code.
4.  `ValidProgram` may throw an exception if the new code violates correctness rules. This error is propagated up to `VMProcess`.
5.  If the code injection is successful, `Loader` passes the updated, high-level program representation from `ValidProgram` to the compiler.
6.  The compiler translates the code into a low-level representation—`LowVMProgram`. This step, thanks to prior validation, is guaranteed to succeed.
7.  `Loader` passes the final `LowVMProgram` to `VMProcess`.

¹Many `VMProcess` processes can be run in the virtual machine. In reality, the API request is received by the `Supervisor` module, which decides to which process (`VMProcess`) to pass it.

### ValidProgram

The main task of this module is to verify the correctness of the entire program and maintain a consistent state. Its main method, `insertCode`, performs transactional code addition. In reality, the operations are performed on a temporary copy of the state. Only when all validation steps are successfully completed is the main state replaced by the working copy. The verification process is as follows:

1.  **Injecting types:** New type definitions are added to the set of types already existing in the program. Potential duplicate names are checked².
2.  **Validating types:** All types in the program (both old and newly added) are analyzed by `TypeValidator`. This allows for the detection of errors that arise from injecting new types (e.g., introducing a type that closes a cycle in the inheritance hierarchy). The detailed operation of the `TypeValidator` module is described in Part IV in Chapter 8.
3.  **Validating and injecting global variables:** Duplicate names are checked, and it is verified whether the types of declared variables exist in the context.
4.  **Validating and injecting functions:** At the beginning of this stage, it is checked whether the name of the injected function has not been duplicated. Then, it is individually analyzed by `FunctionValidator` in the full context of the program's types. If the verification is successful, the new function is added to the program state. The detailed operation of the `FunctionValidator` module is described in Part IV in Chapter 9.
5.  If all the above steps are successful, the internal state is updated, and the operation ends with success. Otherwise, the working copy is discarded, and an exception with error information is thrown.

### Compiler

The compiler is the last module in the loading pipeline. Its task is to translate the high-level, verified program representation into a low-level, executable micro-bytecode (`LowVMProgram`). Since it receives a program with a guarantee of correctness, the compilation process cannot fail and does not need to contain any validation logic, thanks to the guarantees provided by `TypeValidator` and `FunctionValidator`. For each function, this process proceeds as follows:

1.  **Function translation (preprocessing):**
    (a) Names of local variables and arguments are converted to constant offsets relative to the beginning of the stack frame. This process is realized by re-analyzing the control flow graph and calculating offsets on the local stack³.
    (b) The positions of labels in the code are remembered, and the labels themselves are removed from the resulting set of function instructions.

2.  **Translation to micro-bytecode:**
    (a) Jump instructions are updated—symbolic label names are replaced by previously calculated relative offsets in the instruction table.
    (b) The representation of each instruction is changed to its final, numeric form, described in section 6.2. Instruction arguments are converted to their numeric identifiers: variable names to offsets, names of functions, types, and global variables to their corresponding identifiers. Additionally, function metadata needed for the execution module, such as `local_size`, `arg_size`, and `ret_size`, which originally had to be calculated by the user and written explicitly in the bytecode, are calculated.

The resulting `LowVMProgram` is ready to be passed to the execution module. Separating this stage allows for the future introduction of additional optimization steps that generate code optimized for execution. This process is described in more detail in section 19.3.1.

²A correct program requires that the names of all functions, types, global variables, and methods in the program be unique.
³It is assumed that at this stage, all variable names within a function are at the same offset, which is guaranteed by earlier stages.

---

# Chapter 7
# Passing Call Arguments and Interactive Mode (REPL)

After implementing the dynamic code injection mechanism, creating a tool that would fully demonstrate the capabilities of this mechanism became a natural idea. For this purpose, a simple, working version of a read-eval-print loop, known as REPL (Read-Eval-Print Loop), was developed. The most well-known tool that provides a REPL is the Python language, which allows running in an interactive/calculator mode.

The created tool allows the user, within a single, continuous session, to dynamically inject new code, define all entities available in bytecode (types, global variables, functions), and execute single instructions or entire functions with arguments passed to them, while verifying the correctness of each injected piece of code. Thanks to this, testing and verifying the correctness of individual code fragments becomes much faster and simpler. This interactive loop also serves as a basis for integration with tools such as Jupyter Notebooks.

Implementing this tool also directly helped in debugging incorrect bytecode fragments. When writing a program, one could repeatedly paste its code into the REPL and try to inject it into the state. If it was erroneous, we received a nicely formatted verification error message. This eliminated the need to restart the machine with every change in the code. It also made it easier to find errors related to the program state in the loading module (`Loader`).

## 7.1. Supported REPL Syntax

The virtual machine can be launched in interactive mode using the `-r` flag:

```sh
$ ./VM -r
```

After launching, the REPL expects one of the following commands:

-   `{BYTECODE}` — Direct injection of a block of code into the machine without its immediate execution. This allows for the definition of new types, functions, or global variables that will be available in the later part of the session.

-   `!{LIST_OF_INSTRUCTIONS}` — Injection and immediate execution of a block of instructions. The REPL automatically wraps the provided instructions in a temporary function.

-   `!INSTRUCTION` — A shortened version of the above command for a single instruction.

-   `#VARIABLE_NAME TYPE_NAME` — Initialization of a new global variable with the given name and type.

-   `$VARIABLE_NAME` — Prints the current value stored in the global variable to standard output.

-   `FUNCTION_NAME (ARG_1, ARG_2, ...)` — Invokes a previously defined function with the given arguments.

-   `q` — Exits the REPL loop and terminates the machine's operation.

Below is an example of how to use the above commands:

```
$ ./bin/VM -r          # Start the machine in REPL mode.
++++++++++++++++++++++++++++++
+ DuckREPL has started +
++++++++++++++++++++++++++++++
>>> #g i64             # Initialize a global variable named g with type i64.
>>> $g                 # Print the value of variable g, initialized to 0.
0
>>> {                  # Inject the addToGlobal function.
type fun: addToGlobal {i64} void
function addToGlobal {
    init_lany_type x, i64;
    mov_i64_g64 x, g;
    add_i64_i64 x, arg0;
    mov_g64_i64 g, x;
    deinit;
    ret;
}
}
>>> !mov_g64_imm g, 123; # Execute a single instruction
                         # placing the value 123 in variable g.
>>> $g
123                      # Updated value.
>>> addToGlobal(321)     # Call the injected addition function.
addToGlobal -> 0         # The function returns no result, so zero is printed.
>>> $g
444                      # The value of variable g after executing addToGlobal().
>>> q
Exiting REPL             # Closing the session.
```
*Figure 7.1: An example session in REPL mode, demonstrating the use of various commands.*

It should be noted that in the current, simplified version of the implementation, the REPL accepts and returns only integer numbers as arguments and results. This limitation will be removed in the future when the full implementation of the universal `VmValue`, described in Chapter 16, is added.

## 7.2. Implementation

The implementation of the REPL mode and the argument passing mechanism required solving several key problems:

1.  How to pass bytecode typed in the terminal to the machine, whose API was previously adapted only for reading from files?
2.  How to implement the execution of single instructions, given that the machine's execution model is based on calling functions?
3.  How to enable the calling of any function indicated by the user (not just the default `main` function) and how to pass arguments to it?
4.  How to pass the result of a function or program execution back to the user?

### 7.2.1. Problem 1: Passing bytecode from the terminal

In the current architecture, the `Loader` module operates on files. The first solution to this problem would be a simple workaround: the input data from the user is saved to a temporary file with a random, unique name. Then, the path to this file is passed to the existing machine API, which treats it like any other file with source code. The temporary file will be automatically deleted by the operating system after some time.

Alternatively, a desirable improvement would be to extend the parser's capabilities so that it can operate directly on objects of type `std::string` or data streams in memory. This would eliminate the need for file operations and avoid cluttering the file system, and the captured string of characters from the user could be passed directly to the virtual machine.

Due to our implementation being just a simple, working version, the first version of the solution to this problem was implemented.

### 7.2.2. Problem 2: Executing single instructions

Our machine's execution model is based on calling functions—instructions must exist in the context of some function block. Forcing the user to manually define a new function for every single operation in the REPL would be impractical and inconsistent with the classic assumptions of a REPL. We solved this problem as follows. The injected instruction or list of instructions (e.g., via the `!{...}` command) is automatically wrapped in a temporary, dynamically generated function. It takes no arguments and returns no value. For example, for the command: `!mov_g64_imm global, 123;`, the REPL will transform it into the following form before passing the code to the machine:

```
type fun: step_N {} void
function step_N {
    mov_g64_imm global, 123;
    ret;
}
```
Where `N` is the number of the executed step/command. Then, the REPL immediately instructs the machine to execute this very function.

### 7.2.3. Problem 3: Calling functions with arguments

The most difficult problem to solve was enabling the calling of a single function and passing arguments to it. Initially, the virtual machine provided only one API entry point for code execution—`run()`, which took no parameters and assumed the existence of a `main` function in the program. The execution consisted of finding and calling the `main` function, and its result was treated as the program's exit code.

To enable the calling of any function with arguments, we introduced a new API endpoint:
`runFunction(std::string function_name, std::vector<i64> arguments)`

However, the question arose—how to transfer the passed arguments to the virtual machine's memory, specifically to the local stack of the called function?

The solution was inspired by the mechanism of calling the `main` function in the standard C library (`libc`). In binary programs, the `main` function is not the first function to be executed. It is preceded by initialization code (contained, for example, in the `_start` function), which prepares the environment, e.g., builds the `argv` argument table and sets the `argc` counter, and only at the end calls `main`. We implemented an analogous mechanism. For each external function call, a special startup function, named `_vm
