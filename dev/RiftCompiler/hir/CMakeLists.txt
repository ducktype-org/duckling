add_library(HIR
		src/hir/hir.cpp
		src/hir/rift_symbols.cpp
		src/hir/analysis_state.cpp
		src/hir/hir_expr.cpp)

make_module(HIR USES Base SymbolTable PstParser TypeSystem)

rift_add_test(compiler hir_test_simple tests/test.cpp
		USES Exec Base Clap Operations TypeSystem TPC PstParser HIR)
