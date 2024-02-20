add_executable(rift_main src/main.cpp)
target_link_libraries(rift_main Compiler Printer Lexer PstParser Filesystem Clap Base)
