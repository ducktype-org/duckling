\page lir LIR

# What is LIR

LIR is a module responsible for creation and definition of Low Intermediate Representation.
LIR is very similar to MIR but its drops certain abstractions,
which makes it simpler and is one step closer to an executable.

## LIR compared to MIR:

* LIR operates on TSL types (instead of TSH types).
  TSL layouts of types of local variables and function are generated during MIR→LIR lowering.
* Lifetime-flags from MIR are changed into explicit boolean variables
* No operation-flags are present in LIR. Operation flags from MIR are converted into instructions.
* Following special instruction are converted into "proper" code based on generic instructions:
  * `function-end` instruction is converted into proper return instruction
  * Special instruction for destructor handling in MIR are converted into
    checking lifetime flags, and explicit calls to destructors.



