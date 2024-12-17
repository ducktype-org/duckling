===============
Bytecode checks
===============

This file contains a list of static checks performed by
the VM Preprocessor during file loading

Glossary
========

List:

* Stack structure - list of types currently initialized on the stack

List of checks
==============

* Maintain the stack structure between jumps
* Instructions (opcodes) operate on correct types
    * Check if pointer has a correct type assigned
* Function calls have appropriate types on newArgs stack
* Check if operations on ArrPTR have correct offset (sizeof(T))
* Check if pointers are casted correctly (polymorphism)

* Variants...

=============
