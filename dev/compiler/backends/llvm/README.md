\page llvm-backend LLVM Backend

# LLVM backend

This module contains llvm backend for Duckling compiler.
It also encapsulated llvm components in a way, that usage of this module
does not require to include llvm headers.


# LLVM mini doc

Since there is no API doc, that is easy to grasp as a LLVM-beginner, we put here
our "mini docs" that should be helpful to start working with LLVM code.

## Useful resources:

**Official llvm examples:**
<https://github.com/llvm/llvm-project/tree/main/llvm/examples>

**LLVM api source docs**:
<https://llvm.org/doxygen/>

## API vs internals

To my best knowledge LLVM itself does not provide any clear distinguishing
between external API and internal implementations.
Here is a list of things we consider stable-ish external API based on our work with LLVM code so far (also usage of those components is usual quite simple) (note that this list is not complete and will be expanded or modified as we interact more with LLVM code and features):

* Operations on modules as a whole (optimizations, compilation, printing, etc).
* Construction of functions using `llvm::Function::Create` (not of their code).
* Construction of blocks with `llvm::BasicBlock::Create` (not of their code).
* Construction of block code using `llvm::IRBuilder`(one per block).
* Generation of simple types (ints, float, void, ...) using `llvm::Type::getGivenType(context)`.
* Passing around a reference or a pointer to `llvm::Value`, `llvm::Function`, `llvm::Instruction`, `llvm::Context`.


## Important concepts

### Modules

Top level entity of LLVM IR is a module.
Module is a set of function definitions, function declarations, type declarations, 
and some additional data (e.g. debug info).

Modules can be compiled into bitcode/object code, optimized, verified, etc.
Note that similar functionalities can be performed on single functions, but
will usually require more complex usage of LLVM code, that intuitively does not look stable.

### Functions

Functions represent executable code in LLVM IR.

Useful snippet:
```cpp
// verify singe function
bool error_found = llvm::verifyFunction(*fun, &llvm::errs());

// print single function:
fun->print(llvm::outs());
```

### Builders

There are two builders (important to API):

* `llvm::IRBuilder` -- used for building basic blocks
* `llvm::DIBuilder` -- used for building debug information


### Memory management

LLVM seems to manage memory somewhat automatically,
in the sense that most objects will be linked to some parents, e.g.:

* functions to modules
* blocks to functions
* instructions to blocks
* ...

So far LLVM automatically managed linked objects underneath, and 
"we" only had to deal with what top-level object we have at hand -- most likely a module.


### LLVM Values

Values are anything that can be a operation argument.
LLVM values implementation is based on inheritance. 
If something can be a value, it inherit from `llvm::Value`.
For example registers "returned" by instructions are represented
by the pointers to the instruction itself casted to `llvm::Value*`.


## LLVM includes

Currently unused includes, that might be useful in the future:

```cpp
#include <llvm/ADT/APInt.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Host.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/ADT/Optional.h>
```

