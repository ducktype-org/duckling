=====
Tests
=====

HIR currently has a single test, that takes a simple file, passes it through the lexer, parser and HIR, and then checks if values of constants inside the file are correctly calculated.


Currently no SymbolTable unit tests exits. There were some in the past, but they are now no longer valid after a couple refactors.
SymbolTable in now much more directly linked with the HIR module, and probably should not be treated as an independent submodule anymore.

