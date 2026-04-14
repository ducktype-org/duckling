add_executable(duckc src/main.cpp)
target_link_libraries(duckc Printer Lexer PstParser Filesystem Clah Base Diagnostic QueryFramework HELIOS MIR Driver CompilerTimeStats Repl mimalloc)
