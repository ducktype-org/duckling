# Compiler
This is a last step on the loading pipeline.

Compiler 



For each function, this process

proceeds as follows:

1.  **Function translation:**
    - Names of local variables and arguments are converted to constant
    offsets relative to the beginning of the stack frame. This process is
    realized by re-analyzing the control flow graph and calculating
    offsets on the local stack.
    - The positions of labels in the code are saved, and the labels
    themselves are removed from the resulting set of function
    instructions.

2.  **Translation to micro-bytecode:**
    - Jump instructions are updated — symbolic label names are replaced by
    previously calculated relative offsets in the instruction table.
    - The representation of each instruction is changed to its final, numeric
    form. Instruction arguments are converted to their numeric
    identifiers: variable names to offsets, names of functions, types, and
    global variables to their corresponding identifiers. Additionally,
    function metadata needed for the execution module, such as
    `local_size`, `arg_size`, and `ret_size` is calculated.

The resulting `LowVMProgram` is passed to the `VMProcess`. Separating this stage
allows for the future introduction of additional optimization steps that generate
code optimized for execution(Micro-bytecode).