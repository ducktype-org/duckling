add_library(HIR
		src/hir/hir.cpp
		src/hir/rift_symbols.cpp
		src/hir/analysis_state.cpp
		src/hir/hir_expr.cpp)

make_module(HIR USES Base SymbolTable PstParser TypeSystem)

rift_add_test(compiler hir_test_simple tests/test.cpp
		USES Exec Base Clap Operations TypeSystem TPC PstParser HIR)



add_library(SymbolTable
		src/symtable/symtable.cpp
		src/symtable/scope.cpp
		src/symtable/symbol.cpp
		src/symtable/lookup_result.cpp)

make_module(SymbolTable USES Base PstParser TypeSystem HIR)

rift_add_test(compiler symtable_test_simple tests/symtable_test_simple.cpp
		USES SymbolTable PstParser)
