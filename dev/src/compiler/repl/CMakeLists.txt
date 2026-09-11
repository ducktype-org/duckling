if(USE_REPLXX)
    set(REPL_IMPL_SRC
        src/repl/frontend_implementations/replxx.cpp
        src/repl/frontend_implementations/replxx_helpers.cpp
    )
else()
    set(REPL_IMPL_SRC src/repl/frontend_implementations/minimal.cpp)
endif()

add_library(Repl
    src/repl/frontend.cpp
    src/repl/helpers.cpp
    src/repl/session.cpp
    ${REPL_IMPL_SRC}
)


make_module(Repl USES Base
        Filesystem
        QueryFramework
        Driver
        HELIOS
        ModuleTree
        DVMBackend
        OSUtils)

duck_add_test(compiler extract_statement_sources tests/extract_statement_sources_test.cpp USES Repl)
duck_add_test(compiler split_input_into_statements tests/split_input_into_statements_test.cpp USES Repl)
duck_add_test(compiler lookup tests/lookup_test.cpp USES Repl HELIOS)
duck_add_test(compiler repl_session tests/repl_session_test.cpp USES Repl HELIOS)
duck_add_test(compiler repl_statement_helpers tests/repl_statement_helpers_test.cpp USES Repl)
if(USE_REPLXX)
    target_link_libraries(Repl replxx)
    target_compile_definitions(Repl PUBLIC USE_REPLXX)
    duck_add_test(compiler replxx_helpers tests/replxx_helpers_test.cpp USES Repl)
endif()
