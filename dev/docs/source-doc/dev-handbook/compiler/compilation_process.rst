============================
Compilation Process Overview
============================



Single file situation
=====================

Source file phase
-----------------

At first we have a source file e.g. `abc.rift`. It is just a file.


Lexer phase
-----------

Lexer can take single file and change in into :code:`TokenData`.

See: :doc:`/source-doc/source-docs/common/tokenizer/lexer/index`.

Parser phase
------------

Parser can take lexer output and return parse-tree (AST), without any semantically meaningful information.

See: :doc:`/source-doc/source-docs/RiftCompiler/src/pst_parser/index`.


**Everything bellow is experimental or theoretical!**


HIR transformation
-------------------

@TODO

MIR representation
------------------

@TODO

Further compilation
-------------------

@TODO

LIR representation
------------------

@TODO

Code generation to LLVM and RiftBC
----------------------------------


