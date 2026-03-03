add_library(Repl
    src/repl/repl_frontend.cpp
    src/repl/repl_session.cpp
    src/repl/utils.cpp
)

make_module(Repl USES Base
        Filesystem
        QueryFramework
        Driver
        HELIOS
        ModuleTree)

duck_add_test(compiler extract_statement_sources tests/extract_statement_sources_test.cpp USES Repl)
