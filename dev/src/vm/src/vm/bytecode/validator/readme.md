# Bytecode Validation

Files in this directory implement static verification of bytecode functions, globals, and types.
This gives us better guarantees for runtime, which in turn allows
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
Its main method, `tryInsertCode`, performs transactional code addition. In reality,
the operations are performed on a temporary copy of the state. Only when all
validation steps are successfully completed is the main state replaced by the
working copy. The verification process works as follows:



1.  **Forward declare functions:** All functions added to the program state are
    saved in a map which stores a mapping from function name to the function's signature. This serves as a map of forward declarations which are needed
    for type validation. For types such as instantiable (non-abstract) classes we want to statically verify that all of their declared virtual methods are implemented, which is done by looking up if a function declared as an implementation exist in the forward declaration map.
2.  **Injecting types:** New type definitions are added to the set of types
    already existing in the program. Potential duplicate type names are detected.
3.  **Validating types:**
    All types in the program (both old and newly added)
    are analyzed by [`TypeValidator`](type_validator.hpp) (which is described below). This allows for
    the detection of errors that arise from injecting new types (e.g. introducing
    a type that closes a cycle in the inheritance hierarchy or injecting an invalid type).
4.  **Validating and injecting global variables:** All new globals are inserted
    into the program state and verified for correctness. This involves:
    - checking for duplicate names of global variables,
    - checking whether the types of declared global variables exist in the program,
    - if a global variable was declared with a constructor or destructor, we check if the specified function (which serves as a constructor/destructor) exists in the program.
5.  **Validating and injecting functions:**
    This stage is dedicated to statically verifying functions. Each function is individually analyzed by `FunctionValidator` in the full context of the program's types (including the newly injected ones). If the verification is successful, the new function is added to the program state. More detailed explanation of `FunctionValidator`'s functionality is described below.

    Note: Type metadata is now built once during the compilation phase in the `Compiler` module by the `TypeBuilder`, rather than being built temporarily during validation.
6.  If all the above steps are successful, the internal state is updated, and
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


### Instruction specific checks

The `validateArgTypesNonTrivially` function consists of a huge `match`
statement checking each and every instruction. There is no default
branch to make sure new instructions are handled as the machine gets developed.

Primitive types have somewhat unusual semantics. Beside the built in ones,
one can define additional types and end up with many primitive types
of the same size. Each type however offers the same operations, there are
no special types, the builtin `i64` type is not any more "inty" than
a custom-defined `float64` type. The names however do matter, each operation
working on two primitive variables of the same size, like `add_p64_p64`
requires that both arguments are of the same type.

This is also where more detailed checks happen, as an example let's look at the
`structLea_pptr_pptr` instruction extended by `ext_field`. Let's call
the (three) arguments `target`, `src` and `field` respectively.
This instruction roughly corresponds to the following C code: `target = &src->field`.
Since this is the third step of verification we already know that variables
referenced in the first two arguments indeed exist and hold pointers.
We also know that the next instruction is the required extension.
Let `T` be the type of value pointed to by `target`. In this last phase
of checks, we verify that `src` points to a structure (data) of some type `S`
such that `S` holds a field named `field` of type `T`.

## Type Validator

The `type_validator.hpp` and its corresponding `.cpp` file implement the validation
of the DVM type system. The validation is handled by the `ValidProgram` class.
The main entry point is the `TypeContext` class, which aggregates a
collection of high-level type definitions (`TypeOfData`). Any errors found during validation result in a `vm::code::ValidationError` subclass being thrown. The checks can be broadly categorized into hierarchy checks and type-specific checks.
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
The hierarchy checks are run by the `vm::code::detail::validateTypes()` function, which expects a whole set of VM program types (`ObjIdNameMap<TypeOfData>`).

#### Type-Specific Checks

After confirming the hierarchy is a DAG, each **newly added** type definition is checked
individually against a set of rules.
*   **Primitive type**:
    *   **Invalid size**:
    A primitive can't have a size of zero.
*   **Pointer**
    *   **Non-existent component type**:
    A pointer can't reference a type which doesn't exist.
*   **Fixed size table type**
    *   **Non-existent component type**:
    A static table can't store a type that doesn't exist.
*   **Dynamic size table type**
    *   **Non-existent component type**:
    A dynamic table can't store a type that doesn't exist.
*   **Function Type**
    *   **Non-existent component type**:
    All parameter types and the return type declared by the function must exist in the program.
*   **Variant Types**:
    *   **Non-existent component type**:
    All types defined as alternatives in the variant must exist in the program.
    *   **Emptiness of variant alternatives**:
    A variant type must not be empty; it must define at least one possible
    alternative type.
*   **Classes and Data types**:
    *   **Duplicate Fields**:
    A class or data struct cannot have more than one field with the same
    name. This check is performed across the entire inheritance hierarchy;
    a subclass cannot redeclare a field that already exists in a superclass.
*   **Classes and Interface types**:
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
*   **Opaque types**:
    Nothing is verified with opaque types.
This step is done by the `vm::code::detail::validateType()` function, which verifies a single type in the full context of types.
