# Playground
duck_add_playground(panic USES Base)
duck_add_playground(clap USES Clap Printer)
duck_add_playground(count_tokens_cpp USES Filesystem TreeSitter Clap Printer)
duck_add_playground(count_tokens_duckling USES Filesystem TokenFile Clap Printer)
duck_add_playground(fs_parser USES Base ModuleTree Clap)

target_link_libraries(clap_example_cat Base Clap)

duck_add_playground(llvm_example USES)
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
