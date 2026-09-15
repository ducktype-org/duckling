add_executable(duckc src/main.cpp)
target_link_libraries(duckc Printer Lexer PstParser Filesystem Clah Base Diagnostic QueryFramework HELIOS MIR Driver CompilerTimeStats OSUtils Repl Version)

# `--version-verbose` reports which LLVM duckc was built against. The Version module must not know
# about LLVM (VM and duck_ls share it), so the version arrives on duckc's own target instead.
target_compile_definitions(duckc PRIVATE DUCKC_LLVM_VERSION="${LLVM_PACKAGE_VERSION}")

# We want duckc to use custom allocator if specified.
target_link_libraries(duckc allocator_proxy_library)

# During compile-time evaluation with JIT enabled, some symbols must be
# resolved from the executor process's dynamic symbol table.
if(JIT_ENABLED)
    target_link_options(duckc PRIVATE -Wl,--export-dynamic)
endif()
