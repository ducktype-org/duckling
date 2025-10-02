# MIR

# What is MIR

MIR is a module responsible for creation and definition of Middle Intermediate Representation.
It is the representation created from HOUT, and the first non-tree representation.

## MIR representation

MIR is a representation where the code of functions is represented as a set of "basic blocks". Each basic block is a list of operations, and each operation is of the form `[ variable := ] operation(value, value, ..., value)`. In particular, MIR is NOT in any way a tree like structure.

We use this representation for the following purposes:

* Lifetime analysis
  * Destructor code insertion.
  * Static verification of move semantics.
* Static analysis of code reachability.
* (potentially some more static analyses).
* As an intermediate step in the process of compilation to LLVM IR.

## Semantics of MIR

MIR still operates on high level types while having a very low level "feel" which can pose some challenges. As of right now, MIR semantics are not yet fully designed, but the general ideas are described.
In the most high level words, MIR can be described as: 
> non-SSA (single static assignment) CFG (control flow graph) representation enriched with lifetime scope data.

## Lifetime scopes

Since a lot of lifetime analysis happens in MIR, MIR introduces its own so-called lifetime scopes.
Lifetime scopes, are similar in nature to standard program scopes and form a tree like structure (distinct structure per each MIR function).
However, lifetime scopes are not directly tied to HELIOS scopes. The two structures will be similar in most scenarios, but in general MIR lifetime scopes are created during mir-lowering phase based on the HOUT structure.
During this creation three things happen simultaneously:

* MIR lifetime scope tree is generated based on the HOUT tree like representation (it is created within the logic of lowering).
* Each local variable is assigned a scope -- a scope within which it as alive. Note that local variables include variables from Duckling source code, but also all variables associated with temporary values of expression. Such temporary-value variables are created during the MIR lowering phase.
* Each instruction is assigned a scope -- conceptually a scope, that this instruction "happens within".

A variable cannot be used outside its scope.
Destructors are inserted in a following way.
MIR function code is initially created without destructor calls.
Then when in initial code a instruction `A` is followed by instruction `B`, then for all variables that ware live in scope of `A`, but not in scope of `B`, destructors of such variables are inserted between `A` and `B`.


Note that the lifetime analysis of MIR is performed on the variable level, which is consistent with semantics of Duckling.
For example, while a class member is a distinct object, its lifetime is tied directly to lifetime of encapsulating object and cannot be manipulated independently.

### MIR operation

MIR operations are the smallest but also most complicated element of MIR.
Each operation is composed of the following components:

* Optional variable, that the output will be stored to.
* "Operation" that describes what this operation actually does.
* Arguments -- a list of MIR Values.
* Flags that describe what Local Variables this operation constructs/destructs/moves.
* MIR lifetime scope this operation "happens within" (used for lifetime analysis)

Example:

```cpp
// operation calling a constructor on local variable v:
v := call(F)   [construct v] [scope ...] // call takes the function as first argument

// operations calling foo, and moving variable v
call(foo, v)   [move v] [scope  ...]
```

Possible subset of operations:

Operations "Expressions":
* `call(func, func args...)`
* `vcall(func-ptr, func args...)`
* `get_pointer(local/global)`
* `GEP(local, index)`
  (GEP stands for "get element pointer", which is an LLVM instruction)
* add, sub, mul, etc
* `destroy(local)`

Operations "Terminators":
* `jmp(block)`
* `branch(bool, block, block)`
* `return(mir value)`

### MIR functions

MIR Functions are the unit of executable code in MIR.
They are made from the following components:

* name
* parameters
* return type
* code, composed of set of basic blocks with marked entry block
* lifetime scope structure

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

Each local variable in MIR has an implicit, hidden "lifetime flag" associated with the variable. It is a boolean flag that dictates whether this local variable is still "alive". An operation with a move flag for variable `v` sets the lifetime flag of variable `v` to false. This flag is used to decide (in general) at run-time if a given object has to be destroyed when its lifetime scope ends. 
Lifetime flags become real variables in LIR representation (representation that MIR is lowered into). 

Details of how exactly this will be implemented are not yet decided, since classes are still in progress.
Potential ideas:
* `destruct_if` operation that calls destructor only if lifetime flag is set.
* `call_if` operation that calls any function only if lifetime flag is set.
* `branch_if_live` operation branch based of lifetime flag.

The need for lifetime flags comes directly from the need to express the move semantics.


### Problems and future work

* Getting access to local object's members or global values is problematic.
  On the one hand, a classic approach of doing it in low-level representation is to get pointers to members. At first this seems to work very well with Duckling's high level types, as `ref` types describe the semantics of member access very well. This however breaks when a member is already a `ref`, as Duckling does not support builtin ref-s to ref-s (it is supported via std support).
* Value categories and types in MIR
* Match and switch statements in MIR

### Other ideas that were (in the current version) rejected

* Special operations for move, create, etc instead of flags. This was rejected, because we want to have a "move event" happen exactly when it actually is happening.
* Distinguishing of temporary and local variables. This was deemed not necessary, and therefore not worth the effort. Types of local entities (local/temporary/parameters/global/something else) might be brought back at some point to solve some problems or make them easier in the future (like static implementation of static analysis algorithms or introduction of references to non-local objects).
