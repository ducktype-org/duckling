\page mir MIR

# What is MIR

MIR is a module responsible for creation and definition of Middle Intermediate Representation.

## MIR representation

MIR is a representation where the code of functions is represented as a set of "basic blocks". Each basic block is a list of operations, and each operation is of the form `[ variable := ] operation(value, value, ..., value)`. In particular, MIR in NOT in any way a tree like structure.

We use this representation for the following purposes:

* Destructor code insertion (lifetime analysis).
* Static verification of move semantics.
* Static analysis of code reachability.
* (potentially some more static analyses).
* As an intermediate step in the process of compilation to LLVM IR.

## Semitics of MIR

MIR operates still on high level types while having very low level "feel" which can pose some challenges. As of right now MIR semantics are not yet fully designed, but the general ideas are described.
In the most high level words MIR can be described as: 
> non-SSA CFG representation enriched with Scope data.

### MIR operation

MIR operations are the smallest but also most complicated element of MIR.
Each operation is composed of the following components:

* Optional variable, that the output will be stored to.
* "Operation" that describes what this operation actually do.
* Arguments -- a list of MIR Values.
* Flags that describe what Local Variables this operation construct/destructs/moves.
* Helios Scope this operation originates from (used for lifetime analysis)

Example:

```cpp
// operation calling a constructor on local variable v:
v := call(F)   [construct v] [scope of ...] // call takes the function as first argument

// operations calling foo, and moving variable v
call(foo, v)   [move v] [scope of ...]
```

Possible subset of operations:

Operations "Expressions":
* `call(func, func args...)`
* `vcall(func-ptr, func args...)`
* `get_pointer(local/global)`
* `GEP(local, index)`
* add, sub, mul, etc
* `destroy(local)`

Operations "Terminators":
* `jmp(block)`
* `branch(bool, block, block)`
* `return(mir value)`

### MIR functions

MIR Functions are the unit of executable code in MIR.
They are made from following components:

* name
* parameters
* return types
* code, composed of set of basic blocks with marked entry block

### MIR basic blocks

@note see: <https://en.wikipedia.org/wiki/Basic_block>

Each MIR basic block is a list of operations, where the last operation called "terminator" dictates where the control flow moves after the given block. One cannot "jump into" the basic block, only to its beginning. 


### MIR Values and value semantics

Each MIR Value is one of following:

* MIR Local -- local function variable or parameter.
* MIR Global -- global variable/constant/etc.
* Literal Value.
* MIR Function -- reference to another function.
* MIR Block -- reference to another basic block within the same function.

MIR Locals, MIR Globals and literal values are treated as some byte sequence of unknown scheme.
In particular passing MIR Local to an operation argument or assigning operation output to a variable will simply assign bytes as is. Every other operation has to be explicit.


### Lifetime flags and move semantics

Each local variable in MIR has implicit, hidden "lifetime flag". It is a boolean flag, that dictates whether this local variable is still "alive". Operation with move flag sets this flag to false. This flag will be used to decide if destructor have to be called on a given object. Details of how exactly it will be realized are not yet known.

Potential ideas:
* `destruct_if` operation that calls destructor only if lifetime flag is set.
* `call_if` operation that calls any function only if lifetime flag is set.
* `branch_if_live` operation branch based of lifetime flag.

### Problems and future work

* Getting access to local objects members and globals is problematic.
  On one hand, classic approach of doing it in low-level representation is to get pointers to members. At first is seams to work very well with Duckling high level types, as `ref` type describes semantic of member access very well. This however breaks when member is already a `ref`, as Duckling does not support builtin ref-s to ref-s (it is supported via std support).
* Value categories and types in MIR
* Match and switch statements in MIR

### Other ides that were (in current version) rejected for some reasons

* Special operations for move, create, etc instead of flags. This was rejected, because we want to have "move event" happen exactly when it actually is happening.
* Distinguishing of temporary and local variables. This was deemed not necessary, and therefor not worth the effort. It might be brought back to solve some problems in the future.

