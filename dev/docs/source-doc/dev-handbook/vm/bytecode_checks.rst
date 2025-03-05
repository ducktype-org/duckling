===============
Bytecode Checks
===============

Init deinit
-----------

- Missing init or deinit
- Using stack before init
- Using stack after deinit
- Using stack between init and deinit (legal)

Types
-----

- Moving between two data types
.. - Arithmetical operations on pointer type

Pointers
--------

Ref is not implemented yet, so I used `ref_lptr arg0 arg1` as a placeholder. It's equivalent to `arg0 = &arg1` in C.

- Deref a pointer after deinit
- Deref a pointer to a wrong type
- Deref a pointer after deinit and init

Jumps
-----

- Jump into a block
- Jump out of a block
- Jump between blocks
- Skip a block with a jump (legal)