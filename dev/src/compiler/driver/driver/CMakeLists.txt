include(builtins.cmake)
# ^ This exposes:
# - GENERATED_DIR: the directory of the generated builtin headers, and
# - BUILTINS_EMBEDS: the list of generated header filenames in GENERATED_DIR

add_library(Driver
	src_private/driver_private/backend_operations/compile_llvm.cpp
	src_private/driver_private/backend_operations/compile_dvm.cpp

	src_private/driver_private/operations.cpp
	src_private/driver_private/collect_input.cpp
	src_private/driver_private/save_artifacts.cpp
	
	src/driver/module_flags/module_flags.cpp
	src/driver/backend_type.cpp
	src/driver/operations/generic_operations.cpp
	src/driver/initialize.cpp
	src/driver/exit.cpp

	${BUILTINS_EMBEDS}
)

add_dependencies(Driver builtins_embeds)

# To tell the compiler where to look for the generated files
target_include_directories(Driver PRIVATE ${GENERATED_DIR})

# To tell CLion where to look for the generated files
target_sources(Driver PRIVATE ${BUILTIN_HEADERS})

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend DVMBackend LibVM Artifacts GlobalState Lexer Diagnostic Linker ModuleTree CompilerTimeStats)

duck_add_test(compiler driver tests/driver_test.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test1 tests/incremental_mark_nodes_test1.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test2 tests/incremental_mark_nodes_test2.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test3 tests/incremental_mark_nodes_test3.cpp USES Driver Artifacts)

# Ensure process-ordered execution for incremental mark-nodes tests
set_tests_properties(incremental_mark_nodes_test2 PROPERTIES DEPENDS incremental_mark_nodes_test1)
set_tests_properties(incremental_mark_nodes_test3 PROPERTIES DEPENDS incremental_mark_nodes_test2)
