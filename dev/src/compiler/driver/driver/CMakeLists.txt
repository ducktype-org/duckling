add_library(Driver
	src_private/driver_private/backend_operations/compile_llvm.cpp
	src_private/driver_private/backend_operations/compile_dvm.cpp

	src_private/driver_private/operations.cpp
	src_private/driver_private/statistics_private/statistics.cpp
	src_private/driver_private/collect_input.cpp
	src_private/driver_private/save_artifacts.cpp
	
	src/driver/statistics/statistics.cpp
	src/driver/backend_type.cpp
	src/driver/operations/generic_operations.cpp
	src/driver/initialize.cpp
	src/driver/exit.cpp
)

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend DVMBackend LibVM Artifacts GlobalState Lexer Diagnostic Linker Timer)

duck_add_test(compiler driver tests/driver_test.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test1 tests/incremental_mark_nodes_test1.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test2 tests/incremental_mark_nodes_test2.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test3 tests/incremental_mark_nodes_test3.cpp USES Driver Artifacts)

# Ensure process-ordered execution for incremental mark-nodes tests
set_tests_properties(incremental_mark_nodes_test2 PROPERTIES DEPENDS incremental_mark_nodes_test1)
set_tests_properties(incremental_mark_nodes_test3 PROPERTIES DEPENDS incremental_mark_nodes_test2)
