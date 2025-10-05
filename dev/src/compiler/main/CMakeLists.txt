add_executable(duck src/main.cpp)
target_link_libraries(duck Printer Lexer PstParser Filesystem Clah Base Diagnostic QueryFramework HELIOS MIR Driver Timer)
