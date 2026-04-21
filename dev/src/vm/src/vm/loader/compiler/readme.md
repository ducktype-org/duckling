# Compiler
The Compiler is the last module in the whole loading pipeline. Its execution
begins with a validated high level program and its output is the compiled
low level implementation of the program — `LowVMProgram`. This module performs
the translation from Fat-Bytecode to Micro-Bytecode.

The Compiler is a stateful structure which updates the inner program incrementally enabling
for incremental code injection without the need to recompile the whole program each time
a new function is injected.

## Compilation process

1.  **Building type metadata:**
    - New types from the validated `ValidTypeMap` are translated into low-level `TypeMetadata`
    by the `TypeBuilder` module. This is done by calling `vm::code::detail::buildTypeMetadata()`
    for initial creation or `vm::code::detail::rebuildTypeMetadata()` for incremental updates.
    The `TypeMetadata` contains the cross-references and built v-tables for object and interface
    types which are needed for runtime execution. This translation has a few important steps:
        - **Type Resolution**: All type names are resolved to direct references (`TypeRef` or `TypeCRef`). Previously, component types (e.g., types of fields in a data type) were held as a string representing the type name. After this step, all types keep the direct reference to the corresponding type object.
        - **Field Layout**: For classes, the compiler computes the final in-memory layout by creating a flat list of all fields, including those inherited from superclasses.
        - **V-Table Construction**: A critical step for object-oriented types is building the virtual method table. The compiler resolves the inheritance and implementation hierarchy to determine which function implementation corresponds to each virtual method. This v-table is then attached to the type's metadata, enabling dynamic dispatch at runtime.
    - An important note is that the previously existing `TypeMetadata` is not invalidated when
    `rebuildTypeMetadata()` is called. `TypeMetadata` is a structure which holds cross references
    against other types (for example a pointer type holds a direct reference to the inner type
    existing in the `TypeMetadata` set).
    - These type references are also held in the memory module of the VM. Each memory block holds
    a reference to a type it holds. This means that rebuilding the entire type metadata would cause
    memory blocks to hold dangling references.
    - This step also checks for any new methods which may have been declared in object types or
    interfaces. It stores them in the `method_name_pool`. The mappings in this map are needed to
    translate the method names to their IDs when lowering `virtual_call_pptr_method` instructions
    to micro bytecode.

2.  **"Compiling" new globals:**
    - New global variables from the fat-bytecode program are added to the global data set.
    The order of globals in this structure is important. The index in the `std::vector` used
    internally by the `ObjIdNameMap` is the value of the micro bytecode instruction argument
    in opcodes operating on global variables.

3.  **Compiling new functions:**
    - New functions from the high-level `ValidProgram` are translated to their micro bytecode form and added to
    the LowProgram. This process is split into two steps;
      - **Preprocessing:**
        - The positions of labels in the code are saved, and the labels
        themselves are removed from the resulting set of function
        instructions.
        - Names of local variables and arguments are converted to constant
        offsets relative to the beginning of the stack frame. This process is
        realized by re-analyzing the control flow graph and calculating
        offsets on the local stack.
    - **Translation to micro-bytecode:**
        - Jump instructions are updated — symbolic label names are replaced by
        previously calculated relative offsets in the instruction table.
        - The representation of each instruction's arguments is changed to its final, numeric
        form. Instruction arguments are converted to their numeric
        identifiers: variable names to offsets on the local stack, names of functions,
        types, and global variables to their corresponding IDs. Additionally, function
        metadata needed for the execution module, such as `local_size`, `arg_size`, and
        `ret_size` is calculated. An ID in this context is the index in the `ObjIdNameMap`/`StableObjIdNameMap`, which
        internally uses `std::vector`/`std::deque`. This means the order of functions/globals/types
        in these structures is important.
        - Translation of instructions is performed. Each instruction stored in the high representation
        (fat-bytecode) is translated in their micro-bytecode equivalent. This step currently is
        implemented trivially, with each high-instruction having a micro-instruction equivalent, but in the
        future, once the MicroBytecode set gets minified, some high-instructions will be ommited (like `cast_X_X`, which is
        only needed in the static verification phase, will be gone at runtime), some may be split into more than one
        instruction (for example instead of having a separate instruction for taking an element out of a struct
        and a separate instruction for taking an element from a static array, each of those could be split into
        two instructions — one that moves a pointer by some `offset` and the second one which takes the data
        from the underlying pointer). The high-instruction to low-instruction logic can be found
        [here](./instruction_lowering.hpp).


The resulting `LowVMProgram` is passed to the `VMProcess`.
