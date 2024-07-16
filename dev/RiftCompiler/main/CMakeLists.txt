add_executable(duck src/main.cpp)
target_link_libraries(duck Compiler Printer Lexer PstParser Filesystem Clap Base Diagnostic)
