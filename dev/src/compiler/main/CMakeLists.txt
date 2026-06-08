add_executable(duckc src/main.cpp)
target_link_libraries(duckc Printer Lexer Formatter TokenSource PstParser Filesystem Clah Base Diagnostic QueryFramework HELIOS MIR Driver CompilerTimeStats OSUtils Repl)

# We want duckc to use custom allocator if specified.
target_link_libraries(duckc allocator_proxy_library)

# During compile-time evaluation with JIT enabled, some symbols must be
# resolved from the executor process's dynamic symbol table.
if(JIT_ENABLED)
    target_link_options(duckc PRIVATE -Wl,--export-dynamic)
endif()
