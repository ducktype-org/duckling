# Playground
duck_add_playground(panic panic_playground.cpp USES Base)
duck_add_playground(clah clah_playground.cpp USES Clah Printer)
duck_add_playground(count_tokens_cpp count_tokens_cpp_playground.cpp USES Filesystem tree-sitter tree-sitter-cpp Clah Printer)
duck_add_playground(count_tokens_duckling count_tokens_duckling_playground.cpp USES Filesystem TokenSource Clah Printer)
duck_add_playground(fs_parser fs_parser_playground.cpp USES Base ModuleTree Clah)
duck_add_playground(stack_trace stack_trace_playground.cpp USES Base)
duck_add_playground(init init_playground.cpp USES Init)

target_link_libraries(clah_example_cat Base Clah)

duck_add_playground(llvm_example llvm_example_playground.cpp USES)
llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Support
    nativecodegen)
target_include_directories(llvm_example_playground PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(llvm_example_playground ${llvm_libs} llvm_helpers)

duck_add_playground(ffi ffi_playground.cpp USES Base PkgConfig::LIBFFI)
