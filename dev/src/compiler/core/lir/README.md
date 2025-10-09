# LIR

# What is LIR

LIR is a module responsible for creation and definition of Low Intermediate Representation.
LIR is very similar to MIR but it drops certain abstractions,
which makes it simpler and is one step closer to an executable.

## LIR compared to MIR:

* LIR operates on TSL types (instead of TSH types).
  TSL layouts of types of local variables and function are generated during MIR→LIR lowering.
* Information-less values are removed from LIR. Notably, unit constants and variables are removed including unit parameters and arguments. Note that the unit type is not the only information-less type; other examples include the void type (even rarer, but possible) and arrays of units.
* Lifetime-flags from MIR are changed into explicit boolean variables
* No operation-flags are present in LIR. Operation flags from MIR are converted into instructions.
* The following special instruction are converted into "proper" code based on generic instructions:
  * Special instruction for destructor handling in MIR are converted into
    checking lifetime flags, and explicit calls to destructors.
