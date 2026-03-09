# Code Loading
This file presents a brief overview of the architecture of the process of loading,
verifying, and compiling bytecode in the system designed by ZPP 3.2. This file
contains just the overview of the verification stage.

The details of this process are described more throughly in
[Bytecode Validation](../bytecode/validator/readme.md).
[Compiler](compiler/readme.md).

## Design Assumptions
When designing the architecture, we aimed to meet the assumptions we set, which
determined the structure and behavior of the individual system components:
-   **Enabling dynamic code injection:**
    The system must allow for the addition of new code fragments (functions, types,
    global data) to an already running virtual machine process without the need
    for its restart. Every such operation must be transactional — in case of an
    error, the system reverts to the previous, correct state and provides the user
    with the appropriate error.
-   **Support for multi-file programs:**
    The architecture must support programs composed of many separate source files
    and ensure the consistency of the entire program.
-   **Static verification:**
    Every code fragment, before being allowed for execution, goes through
    validation, which checks for structural and logical correctness, as well as
    type compatibility.

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

### Parsed File ([`vm::loader::parser::ParsedFile`](./parser/elements.hpp))
The parser builds a syntax tree based on the token stream, with the `ParsedFile`
structure as its root. Each source file is represented by one such structure. It
is a high-level, direct abstraction of the file's content, containing only
information that can be read from it without deeper semantic analysis. At this
stage, functions, opcodes and global variables are still identified by plain strings
(their names). Types are stored in a `TypeOfData` variant.
Structures like virtual tables (v-tables) are not yet built.

### Code Collection ([`vm::code::CodeCollection`](../bytecode/bytecode.hpp))
After parsing all the files that are part of the program, their contents are
combined into a single `CodeCollection` structure, which represents the source code
and types of the entire program. This structure is the main input for the
validation module. This structure can also serve the role of the input to the VM.
The duckling compiler builds this structure directly and loads it via the `loadCode`
endpoint which allows for skipping the lexing and parsing stage. In
[builders](../bytecode/builders/readme.md) you can read more about a module which
helps in building the `CodeCollection` directly.

This structure stores the program in the fat bytecode form and may contain bytecode which is considered invalid. It serves as a simple container for DVMs code.

The key difference compared to `ParsedFile` is the representation of instructions —
instead of raw strings, a variant (`std::variant`) is used here, which encloses
all possible instructions in the C++ type system.

The types of arguments passed to instructions are represented in a similar way
(by the `vm::opargs::OpCodeArg` variant).

### Valid Program ([`vm::code::ValidProgram`](../bytecode/validator/valid_program.hpp))
`ValidProgram` is the main, high-level representation of the program maintained
within the `Loader` module. Its invariant is the guarantee that the state stored
in it is always correct. Its state can be expanded with the `tryInsertCode` function. This structure, similarly to `CodeCollection`, stores the program in the fat bytecode
form, with the difference being that the program state is guaranteed to be valid. All functions/globals and types stored here had to pass through the static verification phase.
An additional feature is that functions stored in valid program store only the reachable code. Any dead code that could exist in a `CodeCollection` function is removed in the validation phase.
For more detailed explanation see [Bytecode Validation](../bytecode/validator/readme.md).

### Low-level machine program ([`vm::code::LowVMProgram`](../core/thread/low_program/low_program.hpp))
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
`TypeOfData` variant. Each DVM type (a member of the mentioned variant) contains only the information read from the file (e.g., names of base types and component types, method names, field names, etc. are represented as a `string`). The full set of `TypeOfData` types is temporarily stored in the `TypeContext` type set during static verification, where it is validated and translated into a safe, validated intermediate structure called `ValidTypeMap`.

During the [compilation phase](compiler/readme.md),
the `ValidTypeMap` is translated into `TypeMetadata`. Each type is translated to a corresponding `vm::Type` - fully expanded objects, enriched with all
the information needed at runtime, such as built virtual method tables
(v-tables), fields inherited from superclasses, and direct references to
component types (other `Type` objects) instead of their names.

## High-Level Data Flow Architecture

The main component managing the program's life cycle is `VMProcess`. It stores
the current, executable state of the program in the form of `LowVMProgram`.
Each `VMProcess` has its own `Loader` module object, which in turn maintains a
consistent, high-level representation of the same code in the form of a
`ValidProgram` object.

The data flow is initiated by an API request to load code — `loadFiles` or `loadCode`.
1.  `VMProcess` receives the request and passes it to its `Loader` object.
2.  The `Loader` processes the file(s), verifies the code through the
    `TypeValidator` and `FunctionValidator` modules (see [bytecode validation](../bytecode/validator/readme.md)),
    and tries to inject it into its internal state (`ValidProgram`).
3.  If the operation succeeds, the `Loader` updates its state, and passes the
    updated high-level program to the `Compiler`. The `Compiler` compiles newly added functions,
    globals, types expands its internal state with their low-level representations. After
    this step, the compiler returns an updated `LowVMProgram` structure to `VMProcess`.
4.  `VMProcess` replaces its old version of the executable code with the new one.
5.  If an error occurs at any stage of loading or verification, the `Loader`
    interrupts the operation, its state remains unchanged, and a detailed
    description of the error (errors are defined `vm/loader/errors.hpp`) is
    passed to `VMProcess`, which in turn is passed to the user via an API response.
    The executable state in `VMProcess` also does not change.

Basically, executing `loadFiles` or `loadCode` tries to "inject" new code into the current
`VMProcess` state and succeeds only if the whole state (the old state + the
newly injected code) represents a valid program. If this fails, one of the
errors is thrown and the state is not updated. When `VMProcess` receives an
execution request (`run`/`runFunction`), it starts a thread (`VMThread`) on
the currently stored `LowVMProgram` object.

## Low level responsibilities of each module on the pipeline

In this subsection, we have described the responsibilities and internal workings
of the key modules involved in the loading process.

### VMProcess

`VMProcess` is the main module for managing the loading and execution of code.
It handles `loadFiles` and `loadCode` requests coming from the external API (through the supervisor) which may come either from a user or the duckling compiler itself.

-   `VMProcess` keeps the current state of the `LowVMProgram` which is the current
    low level representation (executable on the machine) of the code. It's
    initialized with an empty state.
-   `VMProcess` keeps its own instance of the `Loader` module which is
    responsible for loading the code of this process.

If the `loadFile` request comes from a user, it contains a list of paths to files with
a text representation of the bytecode. Otherwise, if the request comes from
the compiler, it contains code provided immediately in the form of a `vm::code::CodeCollection`
structure, which allows to skip the tokenization and parsing stage. The contents of this request are passed to `VMProcess` loader module which tries to inject the given code into the current state.


### Loader
`Loader` is a module that manages high-level code loading and verification. Each
`Loader` class keeps the high level representation of the same code as the one kept
in the `LowVMProgram` of the `VMProcess` it belongs to. Each code loading
request works as follows:

1.  It receives a list of files or a ready `CodeCollection` structure from
    `VMProcess` (in case the bytecode comes directly from the compiler).
2.  If files were received, it runs a `Tokenizer` and a `Parser` to obtain a
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
executable representation of bytecode (`LowVMProgram`). Since the compiler
receives a program with a guarantee of correctness, the compilation process
cannot fail and does not need to contain any validation logic. The process of
compilation is described in [compiler](./compiler/readme.md).
