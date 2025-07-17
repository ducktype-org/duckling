# Compilation Process Overview

> **Deprecated (9.05.2024)**  
> THIS entire file contains LEGACY information.  
> It will be updated as a follow-up to mission 36.


## Single file situation

### Source file phase

First we have a source file e.g. `abc.duck`. It is just a file.


### Lexer phase

Lexer can take single file and change in into list of tokens represented by `TokenData`.
Each token is the atomic unit of Duckling source code. 

**Implementation docs:** [Lexer implementation](../../../../src/common/tokenizer/lexer/readme.md).


### Parser phase

Parser can take lexer output and return a parse-tree (AST), without any semantically meaningful information.

**Implementation docs:** [Parser implementation](../../../../src/compiler/core/pst_parser/element_class_hierarchy.md).

> **Attention**  
> **Everything bellow is experimental or theoretical!**


### HIR transformation and representation

HIR ("High intermediate representation") is an representation and an algorithm responsible for:

- Performing a lookup on each of the symbols.
- Lowering to Middle Intermediate Representation
- @TODO: what else?

**Details:** [HIR details](hir.md).

**Implementation docs:** [HIR implementation](../../../../src/compiler/core/helios/readme.md).


### Further compilation

@TODO


### MIR representation

@TODO


### LIR representation

@TODO


### Code generation to LLVM and DuckBC

@TODO
