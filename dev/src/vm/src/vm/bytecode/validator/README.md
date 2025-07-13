# Bytecode Validators

Files in this directory implement static verification of bytecode functions
and types, which gives us more assumptions for runtime, which in turn allows
for faster execution omitting many runtime checks.

## Function Validator

The `function_validator.hpp` file provides the `validateAndExtractReachableCode`
function which when given a `vm::code::Function` reference returns a new
`Function` object representing the same function, but with correctness
guarantees and eliminated dead code. Any errors found during the validation
process are signalled by throwing a `vm::code::ValidationError` subclass.
This errors are defined in the `errors.hpp` file.

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
by keeping track of the current stack state modulo the actual values,
we only care about their types. This is done by the helper `LocalStack` class.
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

Primitive types have somewhat unusual semantics. Beside the built in ones
one can define additional ones and end up with many primitive types
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
(now three) the arguments `target`, `src` and `field` respectively.
This instruction roughly corresponds to the following C code: `target = &src->field`.
Since this is the third step of verification we already know that variables
referenced in the first two arguments indeed exist and hold pointers.
We also know that the next instruction is the required extension.
Let `T` be the type of value pointed to by `target`. In this last phase
of checks, we verify that `src` points to a structure (data) of some type `S`
such that `S` holds a field named `field` of type `T`.

## Type Validator

TODO