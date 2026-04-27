include(builtins.cmake)
# ^ This exposes:
# - GENERATED_DIR: the directory of the generated builtin sources, and
# - BUILTINS_EMBEDS: the list of generated source filenames in GENERATED_DIR

add_library(Driver
	src_private/driver_private/backend_operations/compile_llvm.cpp
	src_private/driver_private/backend_operations/compile_dvm.cpp

	src_private/driver_private/operations.cpp
	src_private/driver_private/save_artifacts.cpp
	
	src/driver/module_flags/module_flags.cpp
	src/driver/incremental_utils/collect_input.cpp
	src/driver/backend_type.cpp
	src/driver/debug_info/debug_info.cpp
	src/driver/operations/generic_operations.cpp
	src/driver/initialize.cpp
	src/driver/externs_manifest.cpp
	src/driver/exit.cpp
	src/driver/manifest/utils.cpp
	src/driver/manifest/manifest.cpp
	src/driver/packages/packages.cpp
	src/driver/task/task.cpp
	src/driver/repl_utils/repl_dvm_helpers.cpp
	src/driver/repl_utils/script_helpers.cpp
	src/driver/repl_utils/repl_split_helpers.cpp
	src/driver/repl_utils/repl_statement_helpers.cpp

	${BUILTINS_EMBEDS}
)

add_dependencies(Driver builtins_embeds)

make_module(Driver USES Base QueryFramework SystemCommand HELIOS MIR LIR LLVMBackend DVMBackend LibVM Artifacts GlobalState Lexer Diagnostic Linker Archiver ModuleTree CompilerTimeStats Repl JSON)

duck_add_test(compiler driver tests/driver_test.cpp USES Driver Artifacts)
duck_add_test(compiler manifest_json tests/manifest_json_test.cpp USES Driver)
duck_add_test_custom(compiler incremental_mark_nodes_test1 tests/incremental_mark_nodes_test1.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test2 tests/incremental_mark_nodes_test2.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test3 tests/incremental_mark_nodes_test3.cpp USES Driver Artifacts)
duck_add_test_custom(compiler incremental_mark_nodes_test4 tests/incremental_mark_nodes_test4.cpp USES Driver Artifacts)

# Ensure process-ordered execution for incremental mark-nodes tests
set_tests_properties(incremental_mark_nodes_test2 PROPERTIES DEPENDS incremental_mark_nodes_test1)
set_tests_properties(incremental_mark_nodes_test3 PROPERTIES DEPENDS incremental_mark_nodes_test2)
set_tests_properties(incremental_mark_nodes_test4 PROPERTIES DEPENDS incremental_mark_nodes_test3)
