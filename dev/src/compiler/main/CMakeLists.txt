add_executable(duckc src/main.cpp)
target_link_libraries(duckc Printer Lexer PstParser Filesystem Clah Base Diagnostic QueryFramework HELIOS MIR Driver CompilerTimeStats Repl)

# We want duckc to use custom allocator if specified.
target_link_libraries(duckc allocator_proxy_library)

# In compile time evaludation with JIT enabled some symbols have to be looked up 
# in the executor process's dynamic symbol table.
if(JIT_ENABLED)
    target_link_options(duckc PRIVATE -Wl,--export-dynamic)
endif()
