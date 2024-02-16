============================
Compilation Process Overview
============================

.. contents::
    :depth: 2
    :local:

Single file situation
=====================

Source file phase
-----------------

First we have a source file e.g. :code:`abc.rift`. It is just a file.


Lexer phase
-----------

Lexer can take single file and change in into list of tokens represented by :code:`TokenData`.
Each token is the atomic unit of Rift source code. 

Implementation docs: :doc:`/source-doc/source-docs/common/tokenizer/lexer/index`.

Parser phase
------------

Parser can take lexer output and return a parse-tree (AST), without any semantically meaningful information.

Implementation docs: :doc:`/source-doc/source-docs/RiftCompiler/src/pst_parser/index`.


.. attention:: **Everything bellow is experimental or theoretical!**


HIR transformation and representation
-------------------------------------

HIR ("High intermediate representation") is an representation and an algorithm responsible for:

* Performing a lookup on each of the symbols.
* Lowering to Middle Intermediate Representation
* @TODO: what else?

Details: :doc:`/source-doc/dev-handbook/compiler/hir`.

Implementation docs: :doc:`/source-doc/source-docs/RiftCompiler/src/hir/index`.

Further compilation
-------------------

MIR representation
------------------

@TODO

LIR representation
------------------

@TODO

Code generation to LLVM and RiftBC
----------------------------------

@TODO



