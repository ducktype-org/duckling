add_executable(duck
    src/main.cpp
    src/cli_options.cpp
)
target_link_libraries(duck Printer Lexer PstParser Filesystem Clap Base Diagnostic QueryFramework HELIOS MIR Driver)


# TODO PR: add cli options test
