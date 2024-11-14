\page llvm-backend LLVM Backend

# LLVM backend

This module contains llvm backend for Duckling compiler.
It also encapsulated llvm components in a way, that usage of this module
does not require to include llvm headers.


# Mini llvm docs

Since there is no API doc, that easy to grasp, we put here some of
our "mini docs" that can help in the future.

## Useful resources:

**Official llvm examples:**
https://github.com/llvm/llvm-project/tree/main/llvm/exampless


**LLVM api source docs**:
https://llvm.org/doxygen/

## Important concepts

### Builders

There are two important builders:

* `llvm::DIBuilder` -- used for building modules
* `llvm::IRBuilder` -- used for building basic blocks


### Memory management

LLVM seams to manage memory somewhat automatically,
in the sense that most objects will be linked to some parents, e.g.:

* functions to modules
* blocks to functions
* instructions to blocks
* ...

So far LLVM automatically manged should linked objects underneath, and 
"we" only had to deal with what top-level object we have at hand -- most likely a module.


### LLVMValues

LLVM values api is based on inheritance. 
Is something can be a value, it inherit from `LLVM::value`.
For example registers "returned" by instructions are represented
by the pointer to the instruction itself.


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

