add_library(SymbolTable
		src/symtable/symtable.cpp
		src/symtable/scope.cpp
		src/symtable/symbol.cpp
		src/symtable/lookup_result.cpp)

make_module(SymbolTable USES Base PstParser TypeSystem HIR)

rift_add_test(compiler symtable_test_simple tests/symtable_test_simple.cpp
		USES SymbolTable PstParser)