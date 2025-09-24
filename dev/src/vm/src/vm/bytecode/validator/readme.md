# Bytecode Validation

Files in this directory implement static verification of bytecode functions
and types, which gives us more assumptions for runtime, which in turn allows
for faster execution omitting many runtime checks.

## ValidProgram

The core of the bytecode validation is the `ValidProgram` class defined in
the `valid_program.hpp` file and a corresponding `cpp` file. This is the 
class which keeps the current program state of the VM and it's kept by the
loader class. Its invariant is the guarantee that the state stored in it is
always correct. The main task of this module is to verify the correctness 
of the entire program and maintain a consistent state. 
The initial state of `ValidProgram` is initialized with built-in types, such as `i64`,
`ptr_i64`, `void`, or the type of the `main` function. All builtin types are defined
in the `bytecode/builtin_types.{hpp, cpp}`.
Its main method, `insertCode`, performs transactional code addition. In reality, 
the operations are performed on a temporary copy of the state. Only when all 
validation steps are successfully completed is the main state replaced by the 
working copy. The verification process is as follows:

1.  **Injecting types:** New type definitions are added to the set of types
    already existing in the program. Potential duplicate type names are detected.
2.  **Validating types:** All types in the program (both old and newly added)
    are analyzed by `TypeValidator` (which is described below). This allows for
    the detection of errors that arise from injecting new types (e.g. introducing
    a type that closes a cycle **in** the inheritance hierarchy).
3.  **Validating and injecting global variables:** All new global are inserted
    into the program state. Duplicate names are checked, and it is verified
    whether the types of declared global variables exist in the context.
4.  **Validating and injecting functions:** At the beginning of this stage, it is
    checked whether the name of the injected function has not been duplicated.
    Then, each function is individually analyzed by `FunctionValidator` in the full 
    context of the program's types (including the newly injected ones). If the
    verification is successful, the new function is added to the program state.
    More detailed explanation of `FunctionValidator`'s functionality 
    is described below.
5.  If all the above steps are successful, the internal state is updated, and
    the operation ends with success. Otherwise, the working copy is discarded,
    and an exception with error information is thrown.

## Function Validator

The `function_validator.hpp` file provides the `validateAndExtractReachableCode`
function which, when given a `vm::code::Function` reference, returns a new
`Function` object representing the same function, but with correctness
guarantees and eliminated dead code. Any errors found during the validation
process are signalled by throwing a `vm::code::ValidationError` subclass.
These errors are defined in the `errors.hpp` file.

*Note for connoisseurs*: dead code elimination may seem like an out-of-place
silly optimization, but is actually necessary. Many of the checks require
knowing the stack structure (local variables and their types) during
the execution of each instruction. When an instruction in unreachable,
we cannot run those checks, and so we cannot assure that some assumptions
about the instruction hold. Such assumptions might me relied upon by the compiler
(the last stage of the VM pipeline) and cause crashes or bugs when trying
to compile code for which the assumptions do not hold. Therefore, the unchecked
(dead) code is simply removed at this step.

The implementation in the corresponding `.cpp` file is in the `FunctionValidator`
class. We traverse the control flow graph depth-first simulating the stack
operations e.g. (de)initializing variables, casting, calling functions
by keeping track of the current stack state, which records only the types 
of values present on the stack — not their actual values. This is because 
bytecode validation focuses on ensuring type correctness for each instruction, 
rather than tracking runtime values. The is done by the helper `LocalStack` class. 
We require that each instruction is always executed with some fixed stack
state, if this is not the case, we throw an error.

As we traverse the graph instruction by instruction, once the stack structure
bookkeeping is taken care of, we validate the instruction.
There are three kinds of checks:

### Extension checks

Each instruction can take at most 2 arguments, however some operations
require more data. To solve this, the bytecode has `ext_TYPE` instructions
which provide the *preceding* instruction with an additional argument.
Of course only a few instructions accept extensions and each of them
accept only a single kind of extension. The `validateExtension` functions
makes sure that instruction extensions are sound. Note: these checks require
a bit of type-level programming and so resulted in a wall of templates :/
Some care was put to make adding new extension-requiring (or even *optionally*
extendable) instructions painless. You simply have to add a new `ExtensionMetadata`
template specification, the actual checks need not be changed.

### Generic checks

There are a couple of requirements common to all instructions.
The `validateArgsTypes` function takes care of them. The instruction
representation used by the verifier has typed arguments, so at this
stage we check that when an instruction expects e.g. variable `v`
to be a primitive of a certain size, we check that on the stack the variable
`v` exists and is an appropriately-sized primitive. These checks are limited.
For example, when an instruction expects `p` top be a local pointer,
we only check that such variable indeed exists and is a pointer,
not what type of data it points to.

Primitive types have somewhat unusual semantics. Beside the built in ones,
one can define additional types and end up with many primitive types
of the same size. Each type however offers the same operations, there are
no special types, the builtin `i64` type is not any more "inty" than
a custom-defined `float64` type. The names however do matter, each operation
working on two primitive variables of the same size, like `add_l64_l64`
requires that both arguments are of the same type. This is also checked
by this function.

### Instruction specific checks

The `validateArgTypesNonTrivially` function consists of a huge `match`
statement checking each and every instruction. There is no default
branch to make sure new instructions are handled as the machine gets developed.

This is where more detailed checks happen, as an example let's look at the
`structLea_lptr_lptr` instruction extended by `ext_field`. Let's call
the (three) arguments `target`, `src` and `field` respectively.
This instruction roughly corresponds to the following C code: `target = &src->field`.
Since this is the third step of verification we already know that variables
referenced in the first two arguments indeed exist and hold pointers.
We also know that the next instruction is the required extension.
Let `T` be the type of value pointed to by `target`. In this last phase
of checks, we verify that `src` points to a structure (data) of some type `S'
such that `S` holds a field named `field` of type `T`.

## Type Validator and Type Builder

The `type_validator.hpp` and its corresponding `.cpp` file implement the validation 
of the type system, while the construction is handled by `type_builder.hpp` and its 
`.cpp` file. The main entry point is the `TypeContext` class, which aggregates a 
collection of high-level type definitions (`TypeOfData`) and, upon request, validates 
them and produces a low-level, runtime-ready representation (`TypeMetadata`). This 
representation, created using `type_validator.hpp` and `type_builder.hpp`, 
contains execution-specific attributes such as built v-tables for OOP types. Any errors
found during validation result in a `vm::code::ValidationError` subclass being thrown.

This entire process is split into two main phases, validation and building, which are 
managed by the `validateAndProduceTypeMetadata` function.

### Validation Phase

Before types can be used by the runtime, their definitions must be checked for
correctness. This phase ensures the entire type system is sound. The checks are
comprehensive and can be broadly categorized into hierarchy checks and individual
type check.

#### Hierarchy Checks

These checks validate the relationships between types, primarily focusing on
inheritance and implementation structures.
*   **Cycle Detection**: The validator traverses the `extends` and `implements`
    graph to detect any cyclical dependencies. For example, `ClassA` cannot
    extend `ClassB` if `ClassB` also extends `ClassA`. Such cycles would lead
    to infinite recursion and are forbidden.
*   **Undefined Supertype**: Every type name referenced in an `extends` or
    `implements` clause must correspond to an existing type definition within
    the context. The validator ensures there are no references to non-existent
    parent classes or interfaces.

#### Type-Specific Checks

After confirming the hierarchy is a DAG, each type definition is checked
individually against a set of rules.
*   **Classes and Data Structs**:
    *   **Duplicate Fields**:
    A class or data struct cannot have more than one field with the same
    name. This check is performed across the entire inheritance hierarchy;
    a subclass cannot redeclare a field that already exists in a superclass.
*   **Classes and Interfaces**:
    *   **Duplicate `implements`**:
    A class or interface cannot list the same interface more than once in
    its direct `implements` clause.
    *   **Duplicate Methods**:
    A class or interface cannot declare multiple virtual methods with the
    same name. Similarly, a class cannot provide multiple implementations
    for the same method.
    *   **Method Signatures**:
    All methods (both virtual declarations and implementations) must adhere
    to a specific signature format. The first parameter must always be a
    pointer to the type itself (the `this` pointer).
    *   **Implementation Matching**:
    When a class implements a virtual method, the implementation's signature
    must match the virtual method's signature (same return type and parameter
    types, excluding the `this` pointer which is already validated).
    *   **Implementation Correctness**:
    A class cannot provide an implementation for a method that is not declared
    as a virtual method in its hierarchy (i.e., in itself, its superclasses,
    or its implemented interfaces).
    *   **Completeness for Concrete Classes**:
    A non-abstract class must provide an implementation for every virtual method
    inherited from its superclasses and interfaces. Abstract classes are exempt
    from this rule.
*   **Variant Types**:
    A variant type must not be empty; it must define at least one possible
    alternative type.

### Building Phase

Once all validations pass, the `TypeContext` proceeds to build the `TypeMetadata`
object. This involves `buildTypes` function which translates the high-level, 
declarative `TypeOfData` into the low-level, concrete `vm::Type` representation 
used in the VM's runtime.
*   **Type Resolution**:
    All type names are resolved to direct references (`TypeRef` or `TypeCRef`).
    Previously, component types (e.g. types of fields in a data type) were held
    as a string representing the type name. After this step, all types keep the
    direct reference to the corresponding type object.
*   **Field Layout**:
    For classes, the validator computes the final in-memory layout by creating
    a flat list of all fields, including those inherited from superclasses.
*   **V-Table Construction**:
    A critical step for object-oriented types is building the virtual method
    table (which is a virtual method map in our case). The validator resolves
    the inheritance and implementation hierarchy to determine which function
    implementation corresponds to each virtual method. This v-table is then
    attached to the type's metadata, enabling dynamic dispatch at runtime.
