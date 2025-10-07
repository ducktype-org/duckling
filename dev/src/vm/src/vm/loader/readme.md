# Code Loading
TODOP: Update after refactor changes

This file presents a brief overview of the architecture of the process of loading,
verifying, and compiling bytecode in the system designed by ZPP 3.2. This file
contains just the overview of the verification stage. This process is described in
detail in [Bytecode Validation](../bytecode/validator/readme.md).

## Design Assumptions
When designing the architecture, we aimed to meet the assumptions we set, which
determined the structure and behavior of the individual system components:
-   **Enabling dynamic code injection:**
    The system must allow for the addition of new code fragments (functions, types,
    global data) to an already running virtual machine process without the need
    for its restart. Every such operation must be transactional — in case of an
    error, the system reverts to the previous, correct state.
-   **Support for multi-file programs:**
    The architecture must support programs composed of many separate source files
    and ensure the consistency of the entire program.
-   **Static verification:**
    Every code fragment, before being allowed for execution, goes through
    validation, which checks for structural and logical correctness, as well as
    type compatibility.
-   **Room for future optimizations:**
    The loading process concludes with compilation to a low-level representation,
    called "micro-bytecode." This stage was intentionally separated to allow for
    the future introduction of more advanced optimization techniques.

## Code Representations in the loading Pipeline
As the code flows through the system's modules, it is transformed and stored in
various forms. Each subsequent representation is enriched with additional
information and guarantees, bringing the code closer to an executable form.

### Source File (`fs::FilePath`)
The original and most basic form is a text file containing bytecode in a textual
format. In the future, this format may be extended with a binary variant, which
will eliminate the need for parsing files and reduce their size on disk.

### Token Stream (`tokenizer::TokenSource`)
In the first stage of processing, the source file is transformed by the tokenizer
into a sequence of tokens. The `TokenSource` structure stores not only the tokens
themselves but also information about their location in the source file (line and
column number), which is later used to generate clear and accurate error messages.

### Parsed File (`vm::loader::parser::ParsedFile`)
The parser builds a syntax tree based on the token stream, with the `ParsedFile`
structure as its root. Each source file is represented by one such structure. It
is a high-level, direct abstraction of the file's content, containing only
information that can be read from it without deeper semantic analysis. At this
stage, instructions and types are still identified by character strings (`string`),
and structures like virtual tables (v-tables) are not yet built.

### Code Collection (`vm::code::CodeCollection`)
After parsing all the files that are part of the program, their contents are
combined into a single `CodeCollection` structure, which represents the source code
and types of the entire program. This structure is the main input for the
validation module.

The key difference compared to `ParsedFile` is the representation of instructions —
instead of raw strings, a variant (`std::variant`) is used here, which encloses
all possible instructions in the C++ type system.

The types of arguments passed to instructions are represented in a similar way
(by the `vm::opargs::OpCodeArg` variant).

### Valid Program (`vm::code::ValidProgram`)
`ValidProgram` is the main, high-level representation of the program maintained
within the `Loader` module. Its invariant is the guarantee that the state stored
in it is always correct. It's state can be expanded with the `insertCode` function.
For more detailed explanation see [Bytecode Validation](../bytecode/validator/readme.md).

### Low-level machine program (`vm::code::LowVMProgram`)
After successful validation, `ValidProgram` is passed to the compiler, which
translates it into `LowVMProgram` — a low-level and understandable for the 
execution module representation. At this stage, all symbolic names are replaced 
by their numeric counterparts required for the fast operation of the execution 
module:
-   **Instructions**
    previously stored as variants are translated into indices in an
    implementation table or, if the machine is run in tail-call mode, into
    direct pointers to functions implementing the instruction's behavior.
    This representation is implemented as the `MicroInstruction` structure.
-   **Names of local variables**
    are replaced by their offsets on the local stack.
-   **Names of functions, methods, types, and global variables**
    are converted into unique identifiers (numbers, their indices in respective
    tables) that allow access at runtime.

This form is the final product of the loading pipeline, and it is passed to the
virtual machine's execution module — `VMProcess`.

### Representation of Types
During loading, the way types are represented in the program also changes.
Initially, in the `ParsedFile` structure, types are represented by the
`ParsedType` structure, which contains only information read from the file (e.g.,
names of base types and subtypes as `string`). The next step is conversion to the
`TypeOfData` type, which stores types as variants (`std::variant`), similar to
instructions and argument types. After passing through the validator
(`validateTypes` - see `type_validator.hpp`), types are transformed and added to the `TypeMetadata`
collection, which stores `Type` objects—fully expanded objects, enriched with all
the information needed at runtime, such as built virtual method tables
(v-tables), fields inherited from superclasses, and direct references to
subtypes (other `Type` objects) instead of their names.

## High-Level Data Flow Architecture

The main component managing the program's life cycle is `VMProcess`. It stores
the current, executable state of the program in the form of `LowVMProgram`.
Each `VMProcess` has its own `Loader` module object, which in turn maintains a
consistent, high-level representation of the same code in the form of a
`ValidProgram` object.

The data flow is initiated by an API request to load code - `loadFile`.
1.  `VMProcess` receives the request and passes it to its `Loader` object.
2.  The `Loader` processes the file(s), verifies the code through the
    `TypeContext` (see 'validateTypes' in 'type_validator.hpp') and `FunctionValidator` modules, 
    and tries to inject it into its internal state (`ValidProgram`).
3.  If the operation succeeds, the `Loader` updates its state, compiles a new,
    complete version of the program into the `LowVMProgram` form, and passes it
    to `VMProcess`.
4.  `VMProcess` replaces its old version of the executable code with the new one.
5.  If an error occurs at any stage of loading or verification, the `Loader`
    interrupts the operation, its state remains unchanged, and a detailed
    description of the error (errors are defined `vm/loader/errors.hpp`) is
    passed to `VMProcess`, which in turn is passed to the user via an API response.
    The executable state in `VMProcess` also does not change.

Basically, executing `loadProgram` tries to "inject" new code to the current
`VMProcess` state and succeeds only if the whole state (the old state + the
newly injected code) represents a valid program. If this fails, one of the
errors is thrown and the state is not updated. When `VMProcess` receives an 
execution request (`run`/`runFunction`), it starts a thread (`VMThread`) on 
the currently stored `LowVMProgram` object.

## Low level responsibilities of each module on the pipeline

In this subsection, we have described the responsibilities and internal workings
of the key modules involved in the loading process.

### VMProcess

`VMProcess` is the main module for managing the loading and execution of code,
handling `loadProgram` requests coming from the external API which may come either from a user
or the compiler itself. 

-   `VMProcess` keeps the current state of the `LowVMProgram` which is the current
    low level representation (executable on the machine) of the code. It's
    initialized with an empty state.
-   `VMProcess` keeps its own instance of the `Loader` module which is
    responsible for loading the code of this process.

If the `loadProgram` request comes from a user, it contains a list of paths to files with
a text representation of the bytecode, otherwise, if the request comes from 
the compiler, it contains code provided immediately in the form of a `CodeCollection` 
structure, which allows to skip the tokenization and 
parsing stage. The contents of this request are passed to `VMProcess` loader
module which tries to inject the given code into the current state.


### Loader
`Loader` is a module that manages high-level code loading and verification. Each
`Loader` class keeps the high level representation of the same code as the one kept
in the `LowVMProgram` of the `VMProcess` it belongs to. Each code loading
request works as follows:

1.  It receives a list of files or a ready `CodeCollection` structure from
    `VMProcess` (in case the bytecode comes directly from the compiler).
2.  If files where received, it runs `Tokenizer` and `Parser` to obtain a
    `CodeCollection` representation. Syntax errors are detected at this stage
    and errors are thrown in that case.
3.  Having `CodeCollection`, it tries to inject the new code into its internal
    `ValidProgram` object. This operation performs the actual static
    verification of the injected code and is described in detail in 
    [Bytecode Validation](../bytecode/validator/readme.md).
4.  `ValidProgram` may throw an exception if the new code violates correctness
    rules. This error is propagated up to `VMProcess`.
5.  If the code injection is successful, `Loader` passes the updated, high-level
    program representation from `ValidProgram` to the compiler.
6.  The compiler translates the code into a low-level
    representation - `LowVMProgram`. This step, thanks to prior validation, is
    guaranteed to succeed.
7.  `Loader` passes the final `LowVMProgram` to `VMProcess`.

### Compiler
The compiler is the last module in the loading pipeline. Its task is to
translate the high-level, verified program representation into a low-level,
executable representation of bytecode (`LowVMProgram`). Since it receives a
program with a guarantee of correctness, the compilation process cannot fail and
does not need to contain any validation logic, thanks to the guarantees provided
by `TypeContext` (see 'validateTypes' in 'type_validator.hpp') and `FunctionValidator`. For each function, this process
proceeds as follows:

1.  **Function translation:**
    - Names of local variables and arguments are converted to constant
    offsets relative to the beginning of the stack frame. This process is
    realized by re-analyzing the control flow graph and calculating
    offsets on the local stack.
    - The positions of labels in the code are saved, and the labels
    themselves are removed from the resulting set of function
    instructions.

2.  **Translation to micro-bytecode:**
    - Jump instructions are updated — symbolic label names are replaced by
    previously calculated relative offsets in the instruction table.
    - The representation of each instruction is changed to its final, numeric
    form. Instruction arguments are converted to their numeric
    identifiers: variable names to offsets, names of functions, types, and
    global variables to their corresponding identifiers. Additionally,
    function metadata needed for the execution module, such as
    `local_size`, `arg_size`, and `ret_size` is calculated.

The resulting `LowVMProgram` is passed to the `VMProcess`. Separating this stage
allows for the future introduction of additional optimization steps that generate
code optimized for execution(Micro-bytecode).
