# Playground
duck_add_playground(panic panic_playground.cpp USES Base)
duck_add_playground(clap clap_playground.cpp USES Clap Printer)
duck_add_playground(count_tokens_cpp count_tokens_cpp_playground.cpp USES Filesystem TreeSitter Clap Printer)
duck_add_playground(count_tokens_duckling count_tokens_duckling_playground.cpp USES Filesystem TokenSource Clap Printer)
duck_add_playground(fs_parser fs_parser_playground.cpp USES Base ModuleTree Clap)

target_link_libraries(clap_example_cat Base Clap)

duck_add_playground(llvm_example llvm_example_playground.cpp USES)

execute_process(COMMAND llvm-config --libs Core ExecutionEngine Interpreter MC MCJIT Support nativecodegen
                OUTPUT_VARIABLE llvm_libs
                OUTPUT_STRIP_TRAILING_WHITESPACE)

target_include_directories(llvm_example_playground PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(llvm_example_playground ${llvm_libs} llvm_helpers)
