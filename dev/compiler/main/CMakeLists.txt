add_executable(duck src/main.cpp)
target_link_libraries(duck Printer Lexer PstParser Filesystem Clap Base Diagnostic Config QueryFramework HELIOS MIR Driver)
add_to_coverage(duck)
