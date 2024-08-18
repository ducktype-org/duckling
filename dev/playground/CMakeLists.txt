# Playground
add_executable(panic_playground make_panic.cpp)
add_executable(clap_playground clap_playground.cpp)
add_executable(fs_parser_playground fs_parser_playground.cpp)

target_link_libraries(panic_playground Base)
target_link_libraries(clap_playground Base Clap Printer)
target_link_libraries(clap_example_cat Base Clap)
target_link_libraries(fs_parser_playground Base ModuleTree Clap)

add_executable(llvm_example llvm_example.cpp)
llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Support
    nativecodegen)
target_include_directories(llvm_example PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(llvm_example ${llvm_libs} llvm_helpers)
