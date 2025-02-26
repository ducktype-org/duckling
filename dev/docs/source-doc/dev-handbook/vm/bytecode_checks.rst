===============
Bytecode Checks
===============

Init deinit
-----------

- Using stack before init
- Using stack after deinit
- Moving between two data types
- Using stack between init and deinit (legal)

Pointers
--------

- Deref a pointer after deinit
- Arithmetical operations on pointer type
- Deref a pointer to a wrong type

Jumps
-----

- Jump into a block
- Jump out of a block
- Jump between blocks
- Skip a block with a jump (legal)