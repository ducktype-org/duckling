# Code Loading

This file presents a brief overview of the architecture of the process of loading, verifying, and compiling bytecode in the 
system designed by the ZPP 3.2 

## Design Assumptions
When designing the architecture, we aimed to meet the assumptions we set, which determined the structure 
and behavior of the individual system components:
-   **Enabling dynamic code injection:** 
The system must allow for the addition of new code fragments (functions, types, global data) to an already running 
virtual machine process without the need for its restart. Every such operation must be transactional — in case of an 
error, the system reverts to the previous, correct state.
-   **Support for multi-file programs:** 
The architecture must support programs composed of many separate source files and ensure the consistency of the entire program.
-   **Static verification:**
Every code fragment, before being allowed for execution, goes through validation, which checks for structural and logical 
correctness, as well as type compatibility.
-   **Room for future optimizations:** 
The loading process concludes with compilation to a low-level representation, called "micro-bytecode." This stage was 
intentionally separated to allow for the future introduction of more advanced optimization techniques.

## Code Representations in the loading Pipeline
As the code flows through the system's modules, it is transformed and stored in various forms. Each subsequent representation 
is enriched with additional information and guarantees, bringing the code closer to an executable form.

### Source File (`fs::FilePath`)
The original and most basic form is a text file containing bytecode in a textual format. In the future, this format may be 
extended with a binary variant, which will eliminate the need for parsing files and reduce their size on disk.

### Token Stream (`tokenizer::TokenSource`)
In the first stage of processing, the source file is transformed by the tokenizer into a sequence of tokens. The `TokenSource` 
structure (Figure 6.1) stores not only the tokens themselves but also information about their location in the source file 
(line and column number), which is later used to generate clear and accurate error messages.

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
The parser builds a syntax tree based on the token stream, with the `ParsedFile` structure (Figure 6.2) as its root.
Each source file is represented by one such structure. It is a high-level, direct abstraction of the file's content, 
containing only information that can be read from it without deeper semantic analysis. At this stage, instructions
and types are still identified by character strings (`string`), and structures like virtual tables (v-tables) are 
not yet built.

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
After parsing all the files that are part of the program, their contents are combined into a single `CodeCollection`
structure (Figure 6.3), which represents the source code and types of the entire program. This structure is the main 
input for the validation module.

```cpp
struct CodeCollection {
    std::vector<Function> functions;
    std::vector<TypeOfData> types;
    std::vector<GlobalData> global_data;
};
```
*Figure 6.3: The CodeCollection structure aggregating code from all program files.*

The key difference compared to `ParsedFile` is the representation of instructions — instead of raw strings, a variant 
(`std::variant`) is used here, which encloses all possible instructions in the C++ type system.

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
`ValidProgram` (Figure 6.6) is the main, high-level representation of the program maintained within the `Loader` module. Its 
invariant is the guarantee that the state stored in it is always correct. Any modification attempt (e.g., by injecting new 
code) is a transactional operation.

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

The `insertCode` method first verifies the new code, also taking the old context into account, and only after all checks are 
passed does it update the internal state. In case of an error, the old state remains untouched, and an exception is reported 
to the caller with a description of the error that occurred. The initial state of `ValidProgram` is initialized with 
built-in types, such as `i64`, `ptr_i64`, `void`, or the type of the `main` function, which was described in more 
detail in Chapter 15.

### Micro-bytecode — Low-level machine program (`LowVMProgram`)
After successful validation, `ValidProgram` is passed to the compiler, which translates it into `LowVMProgram` (Figure 6.7) —
a low-level, optimized, and, above all, understandable for the execution module representation, also known as micro-bytecode. 
At this stage, all symbolic names are replaced by their numeric counterparts required for the fast operation of the execution module:

-   **Instructions** 
    previously stored as variants are translated into indices in an implementation table or, if the machine is run in 
    tail-call mode, into direct pointers to functions implementing the instruction's behavior.

-   **Names of local variables** 
    are replaced by their offsets on the local stack.

-   **Names of functions, methods, types, and global variables** 
    are converted into unique identifiers (numbers, their indices in respective tables) that allow access at runtime.

This form is the final product of the loading pipeline, and it is passed to the virtual machine's execution module.

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

Each function in `LowVMProgram` is described by the `FuncData` structure (Figure 6.8), which contains the compiled 
micro-bytecode and the metadata necessary for its execution.

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

The instructions themselves are represented by the `Fix8Instruction` structure (Figure 6.9), which, depending on
the execution mode, stores the instruction as an index in the implementation table or a direct pointer to the
function implementing the instruction.

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
During loading, the way types are represented in the program also changes. Initially, in the `ParsedFile` structure, types are represented
by the `ParsedType` structure, which contains only information read from the file (e.g., names of base types and subtypes as `string`).
The next step is conversion to the `TypeOfData` type, which stores types as variants (`std::variant`), similar to instructions and argument
types. After passing through the validator (`TypeValidator`), types are transformed and added to the `TypeMetadata` collection, which stores 
`Type` objects—fully expanded objects, enriched with all the information needed at runtime, such as built virtual method tables (v-tables), 
fields inherited from superclasses, and direct references to subtypes (other `Type` objects) instead of their names.

## High-Level Data Flow Architecture

The main component managing the program's life cycle is `VMProcess`. It stores the current, 
executable state of the program in the form of `LowVMProgram`. Each `VMProcess` has its own 
`Loader` module object, which in turn maintains a consistent, high-level representation of 
the same code in the form of a `ValidProgram` object.

The data flow is initiated by an API request to load code, e.g., `loadFile`.

1.  `VMProcess` receives the request and passes it to its `Loader` object.

2.  The `Loader` processes the file(s), verifies the code through the `TypeValidator` 
    and `FunctionValidator` modules, and tries to inject it into its internal state (`ValidProgram`).

3.  If the operation succeeds, the `Loader` updates its state, compiles a new, complete version 
    of the program into the `LowVMProgram` form, and passes it to `VMProcess`.

4.  `VMProcess` replaces its old version of the executable code with the new one.

5.  If an error occurs at any stage of loading or verification, the `Loader` interrupts the operation, 
    its state remains unchanged, and a detailed description of the error (errors are defined `src/loader/errors.hppXXXX`)
    is passed to `VMProcess`, which in turn is passed to the user via an API response. The executable state in `VMProcess` 
    also does not change.

Basically, executing `loadProgram` tries to "inject" new code to the current `VMProcess` state and succeeds only if the whole state 
(the old state + the newly injected code) represents a valid program. Is this fails one of the errors (defined in `errors.hpp`) is 
thrown and the state is not updated.

This model guarantees that the virtual machine process always operates on correct and consistent code. When `VMProcess` receives an execution request (`run`/`runFunction`), it starts a thread (`VMThread`) on the currently stored `LowVMProgram` object.

- The key concept which accompanies the loading phase is that the state kept in the `VMProcess` and `Loader` is always valid.

After receiving a new `LowVMProgram` from the `Loader`, `VMProcess` updates its internal executable code state and provides 
the user with information about the successful program loading through an API response. In case of an error, the user is 
provided with information about the loading error, containing a detailed description of the error that occurred. Example 
errors generated by the loading module are described in Appendix A.

## Low level responsibilities of each module on the pipeline

In this subsection, we have described the responsibilities and internal workings of the key modules 
involved in the loading process.

### VMProcess

`VMProcess` is the main module for managing the loading and execution of code, handling requests 
coming from the external API which may come either from a user or a compiler itself.

std::expected<api::Response, api::LoadProgramError> loadProgram(
    const std::variant<std::vector<fs::FilePath>, std::vector<code::CodeCollection>>& source
);

-   If the request comes from a user, it contains a list of paths to files with a text representation of the bytecode.
-   If the request comes from the compiler, it contains code provided immediately in the form of a `CodeCollection` 
structure, which allows skipping the tokenization and parsing stage.

- `VMProcess` keeps the current state of the `LowVMProgram` which is the current low level representation (executable on the machine) 
    of the code. It's initialized with an empty state.
- `VMProcess` keeps it's own instance of the `Loader` module which is responsible for parsing the code of this process. 
- `VMProcess` is initialized with an empty state of both `LowVMProgram` and `ValidProgram`


### Loader
`Loader` is a module that manages high-level code loading and verification. Each `Loader` class keeps the high level 
representation of same code as the one kept in the `LowVMProgram` of the `VMProcess` it belongs to. Each code loading request 
works as follows:

1.  It receives a list of files or a ready `CodeCollection` structure from `VMProcess` (in case the bytecode comes directly 
    from the compiler).
2.  If it received files, it runs `Tokenizer` and `Parser` to obtain a `CodeCollection` representation. Syntax errors are 
    detected at this stage and correct errors are thrown in that case.
3.  Having `CodeCollection`, it tries to inject the new code into its internal `ValidProgram` object. This operation performs 
    the actual static verification of the injected code.
4.  `ValidProgram` may throw an exception if the new code violates correctness rules. This error is propagated up to `VMProcess`.
5.  If the code injection is successful, `Loader` passes the updated, high-level program representation from `ValidProgram` to the compiler.
6.  The compiler translates the code into a low-level representation—`LowVMProgram`. This step, thanks to prior validation, is guaranteed to succeed.
7.  `Loader` passes the final `LowVMProgram` to `VMProcess`.

### ValidProgram
The main task of this module is to verify the correctness of the entire program and maintain a consistent state. Its main method, 
`insertCode`, performs transactional code addition. In reality, the operations are performed on a temporary copy of the state. 
Only when all validation steps are successfully completed is the main state replaced by the working copy. The verification process 
is as follows:

1.  **Injecting types:** New type definitions are added to the set of types already existing in the program. 
    Potential duplicate type names are detected.
2.  **Validating types:** All types in the program (both old and newly added) are analyzed by `TypeValidator`. This allows for the 
    detection of errors that arise from injecting new types (e.g. introducing a type that closes a cycle in the inheritance hierarchy). 
3.  **Validating and injecting global variables:** All new global are inserted into the program state. Duplicate names are checked, and 
    it is verified whether the types of declared global variables exist in the context.j
4.  **Validating and injecting functions:** At the beginning of this stage, it is checked whether the name of the injected function has 
    not been duplicated. Then, it is individually analyzed by `FunctionValidator` in the full context of the program's types (including 
    the newly injected ones). If the verification is successful, the new function is added to the program state. 
5.  If all the above steps are successful, the internal state is updated, and the operation ends with success. Otherwise, the working copy 
    is discarded, and an exception with error information is thrown.


### FunctionValidator
TODO

### TypeValidator
TODO

### Compiler
The compiler is the last module in the loading pipeline. Its task is to translate the high-level, verified program representation into a 
low-level, executable representation of bytecode (`LowVMProgram`). Since it receives a program with a guarantee of correctness, the 
compilation process cannot fail and does not need to contain any validation logic, thanks to the guarantees provided by `TypeValidator` 
and `FunctionValidator`. For each function, this process proceeds as follows:

1.  **Function translation:**
    (a) Names of local variables and arguments are converted to constant offsets relative to the beginning of the stack frame. 
    This process is realized by re-analyzing the control flow graph and calculating offsets on the local stack.
    (b) The positions of labels in the code are saved, and the labels themselves are removed from the resulting set of function instructions.

2.  **Translation to micro-bytecode:**
    (a) Jump instructions are updated — symbolic label names are replaced by previously calculated relative offsets in the instruction table.
    (b) The representation of each instruction is changed to its final, numeric form. Instruction arguments are converted to their numeric 
    identifiers: variable names to offsets, names of functions, types, and global variables to their corresponding identifiers. Additionally, 
    function metadata needed for the execution module, such as `local_size`, `arg_size`, and `ret_size` is calculated.

The resulting `LowVMProgram` is passed to the `VMProcess`. Separating this stage allows for the future introduction of additional optimization 
steps that generate code optimized for execution(Micro-bytecode). 