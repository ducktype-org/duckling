add_library(HIR
		src/hir/hir.cpp
		src/hir/analysis_state.cpp
		src/hir/symtable.cpp
		
		src/hir/symbols/rift_symbols.cpp
		src/hir/symbols/symbol.cpp

		src/hir/scopes/scope.cpp
		
		src/hir/lookup/lookup_result.cpp
		
		src/hir/code_representation/hir_expr.cpp
)

make_module(HIR USES Base PstParser TypeSystem)

rift_add_test(compiler hir_test_simple tests/test.cpp
		USES Exec Base Clap Operations TypeSystem TPC PstParser HIR)

rift_add_test(compiler symtable_test_simple tests/symtable_test_simple.cpp
		USES HIR PstParser)
